
#ifndef TOY_KERNEL_VFS_MEM_FS_H
#define TOY_KERNEL_VFS_MEM_FS_H

#include "fs/vfs.hpp"

namespace fs {

class StdFSNode : public Node {
 public:
  StdFSNode();

  ~StdFSNode() override;

  Node* Open(const char* name, uint32_t flags, uint32_t mode) override;

  uint32_t Read(uint32_t size, uint8_t* buf) override;

  uint32_t Write(uint32_t size, uint8_t* buf) override;

  bool Seek(uint32_t offset, uint32_t origin) override;

  void Close() override;

 private:
  uint32_t m_open_count;
};

}  // namespace fs

#endif  // TOY_KERNEL_VFS_MEM_FS_H
