#include <elf/elf.h>

#include <cstdio>
#include <iostream>
#include <vector>

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

int elf_close_file(Elf32_File* file) {
  STD_ELF* std_elf = (STD_ELF*)file;
  std::fclose(std_elf->raw_file);

  std::free(file);

  return 0;
}
}