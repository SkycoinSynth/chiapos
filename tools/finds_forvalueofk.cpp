// g++ finds_forvalueofk.cpp
// ./a.out

#include <iostream>
#include <string>

int main(){

auto KSTART{18};
auto KEND{20};

  for (auto k{KSTART}; k<=KEND; k++){
    auto SSTART{1};
    auto SEND{65536};
    for (auto s{SSTART}; s<=SEND; s=s*2){
      std::string str = "./ProofOfSpace ";
      str = str + "-f \"plot.dat\" create";
      str = str + " -k " + std::to_string(k) ;
      str = str + " -s " + std::to_string(s);
      str = str + " > /dev/null 2>&1";
  
      const char *command = str.c_str();
      int result = std::system(command);
      if (result == 0){
        std::cout << "\n" << command;
        std::cout << "\nk=" << k;
        std::cout << "\ns=" << s;
        break;
      }
    }
  }
}

