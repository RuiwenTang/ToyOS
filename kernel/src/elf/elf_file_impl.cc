#include "elf/elf_file_impl.hpp"

ElfFileImpl::ElfFileImpl() : m_node() {}

ElfFileImpl::~ElfFileImpl() {
  if (m_node) {
    m_node->Close();

    delete m_node;

    m_node = nullptr;
  }
}

bool ElfFileImpl::OnOpen(const char* file_name) {
  auto root_fs = fs::Node::GetRootNode()->Open("/", 0, 0);

  m_node = root_fs->Open(file_name, 0, 0);

  return m_node != nullptr;
}

void ElfFileImpl::OnClose() {
  if (m_node) {
    m_node->Close();

    delete m_node;
    m_node = nullptr;
  }
}

bool ElfFileImpl::OnSeek(uint32_t offset) {
  if (!m_node) {
    return false;
  }

  return m_node->Seek(offset, SEEK_SET);
}

bool ElfFileImpl::OnRead(char* buf, uint32_t size) {
  if (!m_node) {
    return false;
  }

  return m_node->Read(size, (uint8_t*)buf);
}

uint32_t ElfFileImpl::OnGetFileSize() {
  if (!m_node) {
    return 0;
  }

  return m_node->GetSize();
}
