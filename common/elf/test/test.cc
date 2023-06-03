
#include <elf/elf.h>

#include <cstdio>
#include <iostream>
#include <vector>

#include "elf_file_host.hpp"

struct MemoryBlock {
  uint32_t base;
  std::vector<char> mem;
};

std::vector<MemoryBlock> g_memory;
uint32_t g_symbol_table = 0;
uint32_t g_string_table = 0;

const char* get_p_addr(uint32_t v_addr) {
  for (auto const& block : g_memory) {
    if (block.base < v_addr && block.base + block.mem.size() > v_addr) {
      uint32_t offset = v_addr - block.base;

      return block.mem.data() + offset;
    }
  }

  return nullptr;
}

int main(int argc, const char** argv) {
  std::vector<Elf32_Phdr> p_hdrs;
  uint32_t p_hdrs_count = 0;

  test::ElfFileHost elf_file{};

  if (!elf_file.Open(argv[1])) {
    std::cerr << "cannot open " << argv[1] << std::endl;
    return 1;
  }

  if (!elf_file.IsValid()) {
    std::cerr << "invalid elf file" << std::endl;
    return 1;
  }

  if (!elf_file.EnumPhdr(nullptr, &p_hdrs_count)) {
    std::cerr << "cannot enumerate program headers" << std::endl;
    return 1;
  }

  p_hdrs.resize(p_hdrs_count);

  if (!elf_file.EnumPhdr(&p_hdrs[0], &p_hdrs_count)) {
    std::cerr << "cannot enumerate program headers" << std::endl;
    return 1;
  }

  for (auto const& phdr : p_hdrs) {
    std::cout << std::hex << phdr.p_type << " " << phdr.p_offset << " "
              << phdr.p_vaddr << " " << phdr.p_paddr << " " << phdr.p_filesz
              << " " << phdr.p_memsz << " " << phdr.p_flags << " "
              << phdr.p_align << std::endl;
  }

  return 0;
}
