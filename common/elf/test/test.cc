
#include <elf/elf.h>

#include <cstdio>
#include <iostream>

extern "C" {

typedef struct {
  Elf32_File base;
  std::FILE* raw_file;
} STD_ELF;

int std_elf_seek(Elf32_File* file, uint32_t offset) {
  STD_ELF* std_elf = (STD_ELF*)file;

  std::fseek(std_elf->raw_file, offset, 0);

  return 0;
}

int std_elf_read(Elf32_File* file, char* buf, uint32_t size) {
  STD_ELF* std_elf = (STD_ELF*)file;

  std::fread(buf, size, 1, std_elf->raw_file);

  return 0;
}

Elf32_File* elf_open_file(const char* path) {
  STD_ELF* std_elf = (STD_ELF*)std::malloc(sizeof(STD_ELF));

  std_elf->raw_file = std::fopen(path, "r+");

  if (!std_elf->raw_file) {
    return nullptr;
  }

  std_elf->base.impl_seek = std_elf_seek;
  std_elf->base.impl_read = std_elf_read;

  return (Elf32_File*)std_elf;
}
}

int main(int argc, const char** argv) {
  Elf32_File* elf_file = elf_open_file(argv[1]);

  if (elf_file == nullptr) {
    std::cerr << "failed open elf file: " << argv[1] << std::endl;

    return -1;
  }

  if (elf_check_valid(elf_file) != 0) {
    std::cerr << "Elf header check failed" << std::endl;
    return -2;
  }

  return 0;
}
