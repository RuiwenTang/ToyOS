#include "elf_file_host.hpp"

namespace test {

ElfFileHost::ElfFileHost() : m_file() {}

ElfFileHost::~ElfFileHost() {
  if (m_file.is_open()) {
    m_file.close();
  }
}

bool ElfFileHost::OnOpen(const char* file_name) {
  m_file.open(file_name);

  return m_file.is_open();
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

}  // namespace test