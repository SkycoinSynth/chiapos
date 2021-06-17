// Copyright 2018 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_CPP_BACKUP_RESTORE_HPP_
#define SRC_CPP_BACKUP_RESTORE_HPP_

#include <algorithm>
#include <memory>
#include <fstream>
#include "phase2.hpp"
#include "sort_manager.hpp"
#include "exceptions.hpp"

struct TransitInfo
{
    // Parameters that are required to match while plotting individual phases.
    uint8_t  k;
    uint8_t  *id;
    uint32_t id_len;
    uint32_t num_buckets;
    bool     nobitfield;
    std::vector<uint64_t> table_sizes;
    uint32_t buf_megabytes;
    uint64_t stripe_size;
    uint64_t num_threads;

    TransitInfo()
    : k{}
    , id{}
    , id_len{}
    , num_buckets{}
    , nobitfield{}
    , table_sizes{}
    , buf_megabytes{}
    , stripe_size{}
    , num_threads{}
    { }

    TransitInfo(
        uint8_t  k_,
        const uint8_t  *id_,
        uint32_t id_len_,
        uint32_t num_buckets_,
        bool     nobitfield_,
        std::vector<uint64_t> table_sizes_ = {},
        uint32_t buf_megabytes_ = 0,
        uint64_t stripe_size_ = 0,
        uint64_t num_threads_ = 0)
    : k(k_)
    , id{}
    , id_len(id_len_)
    , num_buckets(num_buckets_)
    , nobitfield(nobitfield_)
    , table_sizes(table_sizes_)
    , buf_megabytes(buf_megabytes_)
    , stripe_size(stripe_size_)
    , num_threads(num_threads_)
    { 
        if (id_ != nullptr and id_len > 0) {
            id = new uint8_t[id_len];
            std::copy(id_, id_ + id_len, id);
        }
    }

    ~TransitInfo()
    {
        if(id) delete [] id;
    }

    void WriteToFile(std::ofstream& out) const
    {
        out.write(reinterpret_cast<const char*>(&k), sizeof(k));
        out.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        out.write(reinterpret_cast<const char*>(id), id_len);
        out.write(reinterpret_cast<const char*>(&num_buckets), sizeof(num_buckets));
        out.write(reinterpret_cast<const char*>(&nobitfield), sizeof(nobitfield));

        uint64_t length{};
        length = table_sizes.size();
        out.write(reinterpret_cast<const char*>(&length), sizeof(length));

        for (auto size : table_sizes) {
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        }

        if (nobitfield) {
            out.write(reinterpret_cast<const char*>(&buf_megabytes), sizeof(buf_megabytes));
            out.write(reinterpret_cast<const char*>(&stripe_size), sizeof(stripe_size));
            out.write(reinterpret_cast<const char*>(&num_threads), sizeof(num_threads));
        }
    }

    void ReadFromFile(std::ifstream& in)
    {
        in.read(reinterpret_cast<char*>(&k), sizeof(k));
        in.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));

        if (id) delete [] id;
        id = new uint8_t[id_len]{};
        in.read(reinterpret_cast<char*>(id), id_len);
        in.read(reinterpret_cast<char*>(&num_buckets), sizeof(num_buckets));
        in.read(reinterpret_cast<char*>(&nobitfield), sizeof(nobitfield));

        uint64_t length{};
        in.read(reinterpret_cast<char*>(&length), sizeof(length));

        uint64_t size{};
        for (; length > 0; length--) {
            in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
            table_sizes.push_back(size);
        }

        if (nobitfield) {
            in.read(reinterpret_cast<char*>(&buf_megabytes), sizeof(buf_megabytes));
            in.read(reinterpret_cast<char*>(&stripe_size), sizeof(stripe_size));
            in.read(reinterpret_cast<char*>(&num_threads), sizeof(num_threads));
        }
    }

    // Compares parameters with the ones provided as arg.
    // Throws exceptions in case of mismatch.
    void Compare(const TransitInfo& cli)
    {
        for (uint32_t count = 0; count < id_len; count++) {
            if(id[count] != cli.id[count]) {
                throw std::invalid_argument("id mismatch between phases"); 
                break;
            }
        }

        if (num_buckets != cli.num_buckets) 
            throw std::invalid_argument("num_buckets mismatch between phases"); 

        if (k != cli.k)
            throw std::invalid_argument("k mismatch between phases");

        if (nobitfield != cli.nobitfield)
            throw std::invalid_argument("nobitfield mismatch between phases");

        if (nobitfield) {
            if (buf_megabytes != cli.buf_megabytes)
                throw std::invalid_argument("buf_megabytes mismatch between phases");
            
            if (stripe_size != cli.stripe_size)
                throw std::invalid_argument("stripe_size mismatch between phases");

            if (num_threads != cli.num_threads)
                throw std::invalid_argument("num_threads mismatch between phases");
        }
    }
};

void BackupPhase1(const TransitInfo& info, 
                  const fs::path& tmp_dirname)
{
    // Write summary of phase 1 to a file.
    std::string filename(fs::path(tmp_dirname) / fs::path("summary.phase1.backup"));
    std::ofstream f(filename);

    if (!f.good())
        throw std::invalid_argument("Error opening " + filename + ".\n");

    info.WriteToFile(f);
    f.close();
}

// Backup Phase-2 results to be used for Phase-3 plotting later.
// Phase-2 results depend on the nobitfield flag. If this flag is set to true,
// Phase-2/sorting is performed in a chunk of (contiguous)memory of memory_size.
// If nobitfield is set to false, sorting is done on file using sort buckets.
// The results are later restored and respectively passed on to Phase-3 for compression.
void BackupPhase2(const TransitInfo& info,
                    const fs::path dirname,
                    const std::shared_ptr<Phase2Results> res,
                    const uint64_t memory_size = 0,
                    const uint8_t* memory = nullptr)
{
    std::string filename(fs::path(dirname) / fs::path("summary.phase2.backup"));
    std::ofstream f(filename);
    if (!f.good())
        throw std::invalid_argument("Error opening " + filename + ".\n");

    info.WriteToFile(f);

    if(info.nobitfield == true) {
        // Backup memory containing in-memory phase-2/sorting results.
        assert(memory != nullptr && memory_size > 0);
        std::string filename2(fs::path(dirname) / fs::path("memory.phase2.backup"));
        std::ofstream fmem(filename2);

        fmem.write(reinterpret_cast<const char*>(&memory_size), sizeof(memory_size));
        fmem.write(reinterpret_cast<const char*>(memory), memory_size);

        fmem.close();
        f.close();
    }
    else {
        res->table7.FlushCache();
        for (auto itr = res->output_files.begin();
                  itr != res->output_files.end();
                  ++itr) {
            (*itr)->BackupBuckets();
        }

        // Will need the bitfield/filter to reconstruct table1(FilteredDisk)
        uint8_t bits_per_byte = 8;
        int64_t filter_bitcount = res->table1.GetFilterSize();
        int64_t filter_size = filter_bitcount / (sizeof(uint64_t) * bits_per_byte);
        uint64_t* filter = new uint64_t[filter_size];
        res->table1.GetFilter(filter, filter_bitcount);

        f.write(reinterpret_cast<char*>(&filter_size), sizeof(filter_size));
        f.write(reinterpret_cast<char*>(filter), filter_size * sizeof(filter_size));
        f.close();

        delete [] filter;
    }
}

std::vector<uint64_t> RestorePhase1(const TransitInfo& prev_info, 
                                    fs::path dirname,
                                    std::vector<FileDisk> &tmp_1_disks)
{
    // Restore summary from phase 1
    std::ifstream f;

    for (auto itr = tmp_1_disks.begin(); itr != tmp_1_disks.end(); itr++) {
        f.open(itr->GetFileName());
        if (!f.good()) {
            throw std::invalid_argument("File :" + itr->GetFileName() + " not imported.\n");
        }
        f.close();
    }

    uint64_t size{};
    fs::path filename = fs::path(dirname) / fs::path("summary.phase1.backup");
    f.open(filename);
    if (!f.good() ) {
            throw std::invalid_argument("File :" + filename.u8string() + " not imported.\n");
    }

    TransitInfo info{};
    info.ReadFromFile(f);
    info.Compare(prev_info);

    for (uint8_t count = 0; count < tmp_1_disks.size(); count++) {
        if (f.read(reinterpret_cast<char *>(&size), sizeof(size))) {
            info.table_sizes.push_back(size);
        }
    }
    f.close();
    fs::remove(filename);

    return info.table_sizes;
}

// Phase-2 results depend on the nobitfield flag. If this flag was set to true,
// Phase-2/sorting was performed in a chunk of (contiguous)memory of memory_size.
// If nobitfield was set to false, sorting was done on file using sort buckets.
// for in-memory sort the results would be in memory.phase2.backup, else
// they would be in plot.dat.p2.t*.tmp sort buckets.
std::shared_ptr<Phase2Results> RestorePhase2(const TransitInfo& prev_info, 
                     const std::string& dirname, 
                     std::vector<FileDisk>& disk_list, 
                     size_t memory_size,
                     uint8_t* dest = nullptr)
{
    // Restore summary from phase 2
    std::string summary_file(fs::path(dirname) / fs::path("summary.phase2.backup"));
    std::ifstream f(summary_file);

    if (!f.good())
        throw std::invalid_argument("Error opening " + summary_file + ".\n");

    TransitInfo info{};
    info.ReadFromFile(f);
    info.Compare(prev_info);

    if (info.nobitfield) {
        assert(dest != nullptr && memory_size > 0);
        std::string memory_file(fs::path(dirname) / fs::path("memory.phase2.backup"));
        std::ifstream fmem(memory_file);
        size_t file_mem_size{};

        fmem.read(reinterpret_cast<char*>(&file_mem_size), sizeof(file_mem_size));
        if (file_mem_size > memory_size)
            throw std::invalid_argument("Memory size mismatch between phases.\n");

        fmem.read(reinterpret_cast<char*>(dest), memory_size);
        fmem.close();
        f.close();

        fs::remove(memory_file);
        fs::remove(summary_file);

        return nullptr;
    }

    int64_t filter_size{}; 
    f.read(reinterpret_cast<char*>(&filter_size), sizeof(filter_size));

    uint64_t *filter = new uint64_t[filter_size]{};
    f.read(reinterpret_cast<char*>(filter), filter_size * sizeof(filter_size));
    f.close();

    // Restore table1 as FilteredDisk 
    int8_t bits_per_byte = 8;
    auto filter_bitcount{ filter_size * sizeof(*filter) * bits_per_byte };
    bitfield table_bitfield{ filter, static_cast<int64_t>(filter_bitcount) };

    int64_t entry_size = EntrySizes::GetMaxEntrySize(info.k, 1/*table_index*/, false);
    BufferedDisk bufdisk{&disk_list[1], fs::file_size(disk_list[1].GetFileName())};
    FilteredDisk table1(std::move(bufdisk), std::move(table_bitfield), entry_size);

    // Restore table7 as buffered Disk.
    entry_size = EntrySizes::GetKeyPosOffsetSize(info.k);
    BufferedDisk table7{&disk_list[7], fs::file_size(disk_list[7].GetFileName())};

    // Restore buckets for tables 2 to 6.
    std::vector<std::unique_ptr<SortManager>> output_files;
    output_files.resize(7 - 2);

    for(auto count = 2; count < 7; count++) {
        std::string filename = "plot.dat.p2.t" + std::to_string(count);

        // All bucket files are backed-up with .backup extension and need to be renamed
        for(uint32_t count = 0; count < info.num_buckets; count++) {
            std::ostringstream bucket_number_padded{};
            bucket_number_padded << std::internal << std::setfill('0') << std::setw(3) << count;

            std::string bucket_file = fs::path(dirname) / fs::path(filename 
                                                    + ".sort_bucket_" 
                                                    + bucket_number_padded.str() 
                                                    + ".tmp");

            std::string backup_file = fs::path(bucket_file + ".backup");
            if(fs::exists(backup_file)) {
                fs::rename(backup_file, bucket_file);
            }
            else {
                throw std::invalid_argument("Error opening " + backup_file + ".\n");
            }
        }

        // Restore buckets from bucket files with write_mode = false (no overwrite/truncate).
        auto sort_manager = std::make_unique<SortManager>(
                                count == 2 ? memory_size : memory_size / 2,
                                info.num_buckets,
                                log2(info.num_buckets),
                                entry_size,
                                dirname,
                                "plot.dat.p2.t" + std::to_string(count),
                                static_cast<uint32_t>(info.k),
                                0,
                                strategy_t::quicksort_last,
                                false);

        output_files[count - 2] = std::move(sort_manager);
    }

    fs::remove(summary_file);
    std::shared_ptr<Phase2Results> res{ new Phase2Results{
                                std::move(table1)
                                , std::move(table7)
                                , std::move(output_files)
                                , std::move(info.table_sizes)
                               }};
    return res;
}
#endif
