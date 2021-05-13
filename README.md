# Chia Proof of Space
![Build](https://github.com/Chia-Network/chiapos/workflows/Build/badge.svg)
![PyPI](https://img.shields.io/pypi/v/chiapos?logo=pypi)
![PyPI - Format](https://img.shields.io/pypi/format/chiapos?logo=pypi)
![GitHub](https://img.shields.io/github/license/Chia-Network/chiapos?logo=Github)

[![Total alerts](https://img.shields.io/lgtm/alerts/g/Chia-Network/chiapos.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Chia-Network/chiapos/alerts/)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/Chia-Network/chiapos.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Chia-Network/chiapos/context:python)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/Chia-Network/chiapos.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Chia-Network/chiapos/context:cpp)

Chia's proof of space is written in C++. Includes a plotter, prover, and
verifier. It exclusively runs on 64 bit architectures. Read the
[Proof of Space document](https://www.chia.net/assets/Chia_Proof_of_Space_Construction_v1.1.pdf) to
learn about what proof of space is and how it works.

## C++ Usage Instructions

### Compile

```bash
# Requires cmake 3.14+

mkdir -p build && cd build
cmake ../
cmake --build . -- -j 6
```

### Static Compilation With glibc

```bash
# Statically compile ProofOfSpace

mkdir -p build && cd build
cmake -DBUILD_PROOF_OF_SPACE_STATICALLY=ON ../
cmake --build . -- -j 6
```

### Statically linking with musl library

The process requires you to have `musl-gcc` in your `$PATH` environment variable.
For installing musl library see this [page](https://www.musl-libc.org/how.html).
The `static` flag is used for linking statically.

#### Installing musl using package managers
##### Ubuntu
```bash
sudo apt-get update
sudo apt-get install musl-tools

```
#### Debian
```bash
apt-get update
apt-get install -y musl-tools
```

```bash
export CC="musl-gcc -static -Os"
mkdir -p build && cd build
cmake ../
make -j 6

```

### Run tests

```bash
./RunTests
```

### CLI usage

```bash
./ProofOfSpace -k 25 -f "plot.dat" -m "0x1234" create
./ProofOfSpace -k 25 -f "final-plot.dat" -m "0x4567" -t TMPDIR -2 SECOND_TMPDIR create
./ProofOfSpace -f "plot.dat" prove <32 byte hex challenge>
./ProofOfSpace -k 25 verify <hex proof> <32 byte hex challenge>
./ProofOfSpace -f "plot.dat" check <iterations>
```
Following is the map of what least values of (`-s or --stripes`) work for value of (`-k or --size`).
Note: `k` should be between `17` and `49` and `s` is a power of `2`.
```bash
k=18, s=2048
k=19, s=1024
k=20, s=2048
```

### Benchmark

```bash
time ./ProofOfSpace -k 25 create
```

## ci Building
The primary build process for this repository is to use GitHub Actions to
build binary wheels for MacOS, Linux (x64 and aarch64), and Windows and publish
them with a source wheel on PyPi. See `.github/workflows/build.yml`. CMake uses
[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
to download [pybind11](https://github.com/pybind/pybind11). Building is then
managed by [cibuildwheel](https://github.com/joerick/cibuildwheel). Further
installation is then available via `pip install chiapos` e.g.


## Starting Docker For Testing
To try the docker for debian based systems, follow commands as given below.

```bash
docker pull debian
docker run -it debian /bin/bash

apt-get update
apt-get install -y ssh git gcc cmake vim python-pip
pip install --upgrade cmake
git clone https://github.com/skycoinsynth/chiapos
cd chiapos

mkdir -p build && cd build
cmake ../
cmake --build . -- -j 6

./ProofOfSpace -k 21 -f "plot.dat" -i "7e1392f6b7a2d113f8fb685a7409c81211748c335e87decf348a4345e07dcb2b" create

md5sum ./plot.dat
```
Note: The target output must be `4c881491d57d0b8817302cb6ce23ff52`, in case it's not that's an error.

