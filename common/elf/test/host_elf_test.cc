
#include <elf/elf_obj.hpp>
#include <iostream>

int main(int argc, const char** argv) {
  auto lib = ElfObject::OpenLib(nullptr, (char*)argv[1]);

  if (!lib) {
    std::cerr << "Failed open elf file : " << argv[1] << std::endl;
    return -1;
  }

  if (!lib->Load(true)) {
    std::cerr << "Failed load elf object" << std::endl;
    return -2;
  }

  delete lib;

  return 0;
}
