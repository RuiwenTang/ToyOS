#include "elf_file_host.hpp"

namespace test {

ElfFileHost::ElfFileHost() : m_file(), m_size(0) {}

ElfFileHost::~ElfFileHost() {
  if (m_file.is_open()) {
    m_file.close();
  }
}

bool ElfFileHost::OnOpen(const char* file_name) {
  m_file.open(file_name);

  if (!m_file.is_open()) {
    return false;
  }

  m_file.seekg(0, std::ios::beg);

  m_size = m_file.tellg();

  return true;
}

void ElfFileHost::OnClose() { m_file.close(); }

bool ElfFileHost::OnSeek(uint32_t offset) {
  m_file.seekg(offset);

  return m_file.good();
}

bool ElfFileHost::OnRead(char* buf, uint32_t size) {
  m_file.read(buf, size);

  return m_file.good();
}

uint32_t ElfFileHost::OnGetFileSize() { return 0; }

}  // namespace test
