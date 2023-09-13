#include <elf/elf_file.hpp>

ElfFile::ElfFile() : m_elf_header() {}

ElfFile::~ElfFile() {}

bool ElfFile::Open(const char* file_name) {
  if (!OnOpen(file_name)) {
    return false;
  }

  return CheckValid();
}

void ElfFile::Close() { OnClose(); }

bool ElfFile::EnumPhdr(Elf32_Phdr* header, uint32_t* count) {
  if (!IsValid()) {
    return false;
  }

  // count
  if (count) {
    *count = m_elf_header.e_phnum;
  }

  if (header) {
    // seek to the first program header
    if (!OnSeek(m_elf_header.e_phoff)) {
      return false;
    }

    // read program headers
    if (!OnRead((char*)header, m_elf_header.e_phnum * sizeof(Elf32_Phdr))) {
      return false;
    }
  }

  return true;
}

uint32_t ElfFile::GetEntryPoint() const {
  if (!IsValid()) {
    return 0;
  }

  return m_elf_header.e_entry;
}

bool ElfFile::Seek(uint32_t offset) {
  if (!IsValid()) {
    return false;
  }

  return OnSeek(offset);
}

bool ElfFile::Read(char* buf, uint32_t size) {
  if (!IsValid()) {
    return false;
  }

  return OnRead(buf, size);
}

uint32_t ElfFile::GetFileSize() {
  if (!IsValid()) {
    return false;
  }

  return OnGetFileSize();
}

bool ElfFile::CheckValid() {
  this->OnSeek(0);

  if (!this->OnRead((char*)&m_elf_header, sizeof(m_elf_header))) {
    // cannot read header
    return false;
  }

  if (m_elf_header.e_ident[EI_MAG0] != ELFMAG0 ||
      m_elf_header.e_ident[EI_MAG1] != ELFMAG1 ||
      m_elf_header.e_ident[EI_MAG2] != ELFMAG2 ||
      m_elf_header.e_ident[EI_MAG3] != ELFMAG3) {
    // invalid magic number
    return false;
  }

  // check architecture
  if (m_elf_header.e_machine != EM_386) {
    // for now only support x86
    return false;
  }

  // check file class
  if (m_elf_header.e_ident[EI_CLASS] != ELFCLASS32) {
    // for now only support 32-bit
    return false;
  }

  // check file type
  if (m_elf_header.e_type != ET_EXEC) {
    // for now only support executable
    return false;
  }

  m_is_valid = true;

  return true;
}