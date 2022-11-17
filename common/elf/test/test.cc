
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

int32_t find_global_symbol_table(std::vector<Elf32_Shdr> const& headers) {
  for (int32_t i = 0; i < headers.size(); i++) {
    if (headers[i].sh_type == SHT_DYNSYM) {
      return i;
    }
  }

  return -1;
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

  std::vector<Elf32_Phdr> p_hdrs;
  uint32_t p_hdrs_count = 0;

  elf_enum_phdr(elf_file, nullptr, &p_hdrs_count);
  p_hdrs.resize(p_hdrs_count);
  elf_enum_phdr(elf_file, p_hdrs.data(), &p_hdrs_count);

  std::vector<Elf32_Shdr> s_hdrs;
  uint32_t s_shrs_count = 0;

  elf_enum_shdr(elf_file, nullptr, &s_shrs_count);
  s_hdrs.resize(s_shrs_count);
  elf_enum_shdr(elf_file, s_hdrs.data(), &s_shrs_count);

  // find dynmaic symbol table
  int32_t dym_index = find_global_symbol_table(s_hdrs);
  if (dym_index >= 0) {
    Elf32_Sym symb;
    elf_file->impl_seek(elf_file, s_hdrs[dym_index].sh_offset);
    elf_file->impl_read(elf_file, (char*)&symb, sizeof(Elf32_Sym));

    std::cout << "type = " << symb.st_name << std::endl;
  }

  elf_close_file(elf_file);

  return 0;
}
