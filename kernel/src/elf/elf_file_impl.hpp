#ifndef TOY_KERNEL_ELF_ELF_FILE_HPP
#define TOY_KERNEL_ELF_ELF_FILE_HPP

#include <elf/elf_file.hpp>

#include "fs/vfs.hpp"

class ElfFileImpl : public ElfFile {
 public:
  ElfFileImpl();
  ~ElfFileImpl() override;

 protected:
  bool OnOpen(const char* file_name) override;

  void OnClose() override;

  bool OnSeek(uint32_t offset) override;

  bool OnRead(char* buf, uint32_t size) override;

  uint32_t OnGetFileSize() override;

 private:
  fs::Node* m_node;
};

#endif  // TOY_KERNEL_ELF_ELF_FILE_HPP