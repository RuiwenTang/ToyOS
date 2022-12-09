#include <elf/elf_obj.hpp>

ElfObject::ElfObject(const char* path)
    : m_lib_path(path), m_elf_file(nullptr) {}

bool ElfObject::Load(bool is_root) {
  if (m_elf_file) {
    return false;
  }

  m_elf_file = elf_open_file(m_lib_path);

  if (m_elf_file == nullptr) {
    return false;
  }

  if (!CheckHeader()) {
    return false;
  }

  if (!LoadPhdrs()) {
    return false;
  }

  if (is_root) {
    if (!this->OnAllocateMemory(&m_allocaed_base, &m_allocated_size)) {
      return false;
    }

  } else {
    if (!m_root->OnAllocateMemory(&m_allocated_size, &m_allocated_size)) {
      return false;
    }
  }

  m_current_brk = m_allocaed_base + m_allocated_size;

  if (!LoadIntoMemory()) {
    return false;
  }

  if (!ReadDynamicTable()) {
    return false;
  }

  return true;
}

bool ElfObject::CheckHeader() { return elf_check_valid(m_elf_file) == 0; }

bool ElfObject::LoadPhdrs() {
  if (elf_enum_phdr(m_elf_file, nullptr, &m_phdr_count) != 0) {
    return false;
  }

  if (m_phdr_count > 0) {
    if (!this->OnResizePhdrs(m_phdr_count)) {
      return false;
    }

    if (elf_enum_phdr(m_elf_file, this->OnGetPhdrs(), &m_phdr_count) != 0) {
      return false;
    }
  }

  CalculateMemSize();

  if (GetMemorySize() == 0) {
    return false;
  }

  return true;
}

bool ElfObject::LoadIntoMemory() {
  auto p_headers = this->OnGetPhdrs();

  if (p_headers == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < m_phdr_count; i++) {
    if (p_headers[i].p_type == PT_LOAD) {
      if (m_elf_file->impl_seek(m_elf_file, p_headers[i].p_offset) != 0) {
        return false;
      }

      void* p_addr = this->OnVirtualToPhy(p_headers[i].p_vaddr);

      if (m_elf_file->impl_read(m_elf_file, (char*)p_addr,
                                p_headers[i].p_filesz) != 0) {
        return false;
      }
    }
  }

  return true;
}

void ElfObject::CalculateMemSize() {
  uint32_t base = 0xffffffff;
  uint32_t v_brk = 0;

  auto phdrs_head = this->OnGetPhdrs();

  for (uint32_t i = 0; i < m_phdr_count; i++) {
    if (phdrs_head[i].p_type != PT_LOAD) {
      continue;
    }

    if (phdrs_head[i].p_vaddr < base) {
      base = phdrs_head[i].p_vaddr;
    }

    if (phdrs_head[i].p_vaddr + phdrs_head[i].p_memsz > v_brk) {
      v_brk = phdrs_head[i].p_vaddr + phdrs_head[i].p_memsz;
    }
  }

  if (base == 0xffffffff) {
    // zero phdrs
    return;
  }

  m_mem_base = base;
  m_mem_size = v_brk - base;
}

bool ElfObject::ReadDynamicTable() {
  auto p_headers = this->OnGetPhdrs();
  for (uint32_t i = 0; i < m_phdr_count; i++) {
    if (p_headers[i].p_type == PT_DYNAMIC) {
      m_dyn_count = p_headers[i].p_filesz / sizeof(Elf32_Dyn);
      if (!this->OnAllocateElfDyn(m_dyn_count)) {
        return false;
      }

      if (m_elf_file->impl_seek(m_elf_file, p_headers[i].p_offset) != 0) {
        return false;
      }

      if (m_elf_file->impl_read(m_elf_file, (char*)this->OnGetDyns(),
                                p_headers[i].p_filesz) != 0) {
        return false;
      }
    }

    auto dyn_ptr = this->OnGetDyns();
    for (uint32_t j = 0; j < m_dyn_count; j++) {
      if (dyn_ptr[j].d_tag == DT_NULL) {
        // reach the end
        break;
      }

      switch (dyn_ptr[j].d_tag) {
        case DT_HASH:
          m_hash = static_cast<uint32_t*>(
              this->OnVirtualToPhy(dyn_ptr[j].d_un.d_val));
          break;
        case DT_STRTAB:
          m_string_table =
              static_cast<char*>(this->OnVirtualToPhy(dyn_ptr[j].d_un.d_val));
          break;
        case DT_SYMTAB:
          m_symbol_table = static_cast<Elf32_Sym*>(
              this->OnVirtualToPhy(dyn_ptr[j].d_un.d_val));
          break;
        case DT_STRSZ:
          m_string_table_size = dyn_ptr[j].d_un.d_val;
          break;
      }
    }

    // now query all required library
    EnumerateRequiredLib(dyn_ptr, m_dyn_count, nullptr, &m_lib_count);
    if (m_lib_count == 0) {
      continue;
    }
    if (!OnAllocateLibNames(m_lib_count)) {
      continue;
    }
    EnumerateRequiredLib(dyn_ptr, m_dyn_count, OnGetLibNames(), &m_lib_count);
  }
  return true;
}

void ElfObject::EnumerateRequiredLib(Elf32_Dyn* dyn, uint32_t count,
                                     char** names, uint32_t* name_count) {
  uint32_t temp_count = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (dyn[i].d_tag == DT_NEEDED) {
      temp_count++;

      if (names) {
        names[i] = m_string_table + dyn[i].d_un.d_val;
      }
    }
  }

  if (name_count) {
    *name_count = temp_count;
  }
}
