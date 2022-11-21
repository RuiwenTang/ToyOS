
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

struct MemoryBlock {
  uint32_t base;
  std::vector<char> mem;
};

std::vector<MemoryBlock> g_memory;

const char* get_p_addr(uint32_t v_addr) {
  for (auto const& block : g_memory) {
    if (block.base < v_addr && block.base + block.mem.size() > v_addr) {
      uint32_t offset = v_addr - block.base;

      return block.mem.data() + offset;
    }
  }

  return nullptr;
}

int32_t find_global_symbol_table(std::vector<Elf32_Shdr> const& headers) {
  for (int32_t i = 0; i < headers.size(); i++) {
    if (headers[i].sh_type == SHT_DYNSYM) {
      return i;
    }
  }

  return -1;
}

void load_p_headers(Elf32_File* elf_file,
                    std::vector<Elf32_Phdr> const& headers) {
  for (auto const& p_head : headers) {
    if (p_head.p_type == PT_LOAD) {
      std::cout << "load file at : [" << std::hex << p_head.p_paddr
                << "] with size:  " << std::hex << p_head.p_memsz << std::endl;

      MemoryBlock block;
      block.base = p_head.p_paddr;
      block.mem.resize(p_head.p_memsz);

      elf_file->impl_seek(elf_file, p_head.p_offset);
      elf_file->impl_read(elf_file, block.mem.data(), p_head.p_filesz);

      g_memory.emplace_back(block);
    }
  }
}

void check_dynamic_table(Elf32_File* elf_file,
                         std::vector<Elf32_Phdr> const& headers) {
  const char* string_table = nullptr;
  for (auto const& p_head : headers) {
    if (p_head.p_type == PT_DYNAMIC) {
      uint32_t dyn_count = p_head.p_filesz / sizeof(Elf32_Dyn);
      std::cout << "dynamic header with : [" << dyn_count << "] table count"
                << std::endl;

      std::vector<Elf32_Dyn> dyns(dyn_count);

      elf_file->impl_seek(elf_file, p_head.p_offset);
      elf_file->impl_read(elf_file, (char*)dyns.data(), p_head.p_filesz);

      for (auto const& dyn : dyns) {
        if (dyn.d_tag == DT_STRTAB) {
          std::cout << "string table at : " << std::hex << dyn.d_un.d_val
                    << std::endl;
          string_table = get_p_addr(dyn.d_un.d_val);
        } else if (dyn.d_tag == DT_SYMTAB) {
          std::cout << "symbol table at : " << std::hex << dyn.d_un.d_val
                    << std::endl;
        } else if (dyn.d_tag == DT_HASH) {
          uint32_t* p = (uint32_t*)get_p_addr(dyn.d_un.d_val);
          std::cout << "symbol table size at = " << std::hex << dyn.d_un.d_val
                    << std::endl;
          std::cout << "symbol table size = " << p[1] << std::endl;
        } else if (dyn.d_tag == DT_STRSZ) {
          std::cout << "string table size = " << dyn.d_un.d_val << std::endl;
        } else if (dyn.d_tag == DT_INIT) {
          std::cout << "init func at " << dyn.d_un.d_val << std::endl;
        }
      }

      for (auto const& dyn : dyns) {
        if (dyn.d_tag == DT_NEEDED) {
          std::cout << "need lib: [" << (string_table + dyn.d_un.d_val) << "]"
                    << std::endl;
        } else if (dyn.d_tag == DT_RPATH) {
          std::cout << "rpath : [" << (string_table + dyn.d_un.d_val) << "]"
                    << std::endl;
        }
      }
    }
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

  load_p_headers(elf_file, p_hdrs);
  check_dynamic_table(elf_file, p_hdrs);

  elf_close_file(elf_file);

  return 0;
}
