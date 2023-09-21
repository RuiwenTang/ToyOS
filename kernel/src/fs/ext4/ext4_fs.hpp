#ifndef TOY_KERNEL_FS_EXT4_FS_HPP
#define TOY_KERNEL_FS_EXT4_FS_HPP

#include <ext4.h>

#include "fs/vfs.hpp"

namespace fs {

class Ext4FSNode : public Node {
 public:
  Ext4FSNode(const char* name, ext4_blockdev* dev);
  ~Ext4FSNode() override = default;

  Node* Open(const char* name, uint32_t flags, uint32_t mode) override;

  uint32_t Read(uint32_t size, uint8_t* buf) override;

  uint32_t Write(uint32_t size, uint8_t* buf) override;

  bool Seek(uint32_t offset, uint32_t origin) override;

  void Close() override;

  static Ext4FSNode* Mount(char* name);

 private:
  ext4_blockdev* m_blockdev;
};

class Ext4FileNode : public Node {
 public:
  Ext4FileNode(const char* name, ext4_file* file);
  ~Ext4FileNode() override;

  Node* Open(const char* name, uint32_t flags, uint32_t mode) override {
    return nullptr;
  }

  uint32_t Read(uint32_t size, uint8_t* buf) override;

  uint32_t Write(uint32_t size, uint8_t* buf) override;

  bool Seek(uint32_t offset, uint32_t origin) override;

  void Close() override;

 private:
  ext4_file* m_file;
};

}  // namespace fs

#endif  // TOY_KERNEL_FS_EXT4_FS_HPP
