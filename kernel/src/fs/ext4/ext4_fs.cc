
#include "fs/ext4/ext4_fs.hpp"

#include <driver/pci/ide.h>
#include <ext4_mbr.h>
#include <stddef.h>
#include <umm_malloc.h>

extern "C" {

void* ext4_user_malloc(size_t size) { return umm_malloc(size); }

void* ext4_user_calloc(size_t num, size_t size) {
  return umm_calloc(num, size);
}

void* ext4_user_realloc(void* ptr, size_t size) {
  return umm_realloc(ptr, size);
}

void ext4_user_free(void* ptr) { umm_free(ptr); }
}

namespace fs {

#define SECTOR_SIZE 512

IDEDevice* g_device = nullptr;
uint32_t g_device_index = -1;

uint8_t g_buffer[SECTOR_SIZE];

Ext4FSNode* g_root_ext4_node = nullptr;

extern "C" int32_t ide_dev_open(ext4_blockdev* bdev) {
  if (g_device == nullptr) {
    return ENODEV;
  }

  return EOK;
}

extern "C" int32_t ide_dev_bread(ext4_blockdev* bdev, void* buf,
                                 uint64_t blk_id, uint32_t blk_cnt) {
  if (g_device == nullptr) {
    return ENODEV;
  }

  if (ide_ata_access(0, g_device_index, blk_id, blk_cnt, (uint8_t*)buf)) {
    return ENODATA;
  }

  return EOK;
}

extern "C" int32_t ide_dev_bwrite(ext4_blockdev* bdev, const void* buf,
                                  uint64_t blk_id, uint32_t blk_cnt) {
  if (g_device == nullptr) {
    return ENODEV;
  }

  if (ide_ata_access(1, g_device_index, blk_id, blk_cnt, (uint8_t*)buf)) {
    return ENODEV;
  }

  return EOK;
}

extern "C" int32_t ide_dev_close(ext4_blockdev* bdev) { return EOK; }

ext4_blockdev_iface g_i_face{};
ext4_mbr_bdevs g_mbr_bdevs{};

Ext4FSNode* Ext4FSNode::Mount(char* name) {
  IDEDevice* ide_device = ide_get_devices();

  // scan all ide devices
  for (size_t i = 0; i < 4; i++) {
    IDEDevice* device = ide_device + i;
    if (device->reserved) {
      // current only one ide device is used
      g_device = device;
      g_device_index = i;
      break;
    }
  }

  if (g_device == nullptr) {
    return nullptr;
  }

  g_i_face.open = ide_dev_open;
  g_i_face.bread = ide_dev_bread;
  g_i_face.bwrite = ide_dev_bwrite;
  g_i_face.close = ide_dev_close;
  g_i_face.ph_bsize = 512;
  g_i_face.ph_bbuf = g_buffer;
  g_i_face.ph_bcnt = g_device->size;

  // scam mbr

  ext4_blockdev temp_dev{};
  temp_dev.bdif = &g_i_face;
  temp_dev.part_offset = 0;
  temp_dev.part_size = g_device->size * SECTOR_SIZE;

  auto r = ext4_mbr_scan(&temp_dev, &g_mbr_bdevs);

  if (r != EOK) {
    return nullptr;
  }

  // regist this dev
  r = ext4_device_register(&g_mbr_bdevs.partitions[0], "ext4_fs");
  if (r != EOK) {
    return nullptr;
  }
  // mount
  r = ext4_mount("ext4_fs", "/", false);
  if (r != EOK) {
    return nullptr;
  }

  g_root_ext4_node = new Ext4FSNode(name, &g_mbr_bdevs.partitions[0]);

  return g_root_ext4_node;
}

Ext4FSNode::Ext4FSNode(const char* name, ext4_blockdev* dev)
    : Node(name, 0, 0), m_blockdev(dev) {
  SetSize(m_blockdev->part_size - m_blockdev->part_offset);
}

Node* Ext4FSNode::Open(const char* name, uint32_t flags, uint32_t mode) {
  ext4_file* file = new ext4_file;

  auto r = ext4_fopen(file, name, "r");

  if (r != EOK) {
    delete file;
    return nullptr;
  }

  return new Ext4FileNode(name, file);
}

uint32_t Ext4FSNode::Read(uint32_t offset, uint32_t size, uint8_t* buf) {
  return 0;
}

uint32_t Ext4FSNode::Write(uint32_t offset, uint32_t size, uint8_t* buf) {
  return 0;
}

bool Ext4FSNode::Seek(uint32_t offset) { return false; }

void Ext4FSNode::Close() {}

// -----------------------------------------------------------------------------

Ext4FileNode::Ext4FileNode(const char* name, ext4_file* file)
    : Node(name, 0, 0), m_file(file) {}

Ext4FileNode::~Ext4FileNode() {
  if (m_file != nullptr) {
    ext4_fclose(m_file);

    delete m_file;
  }
}

uint32_t Ext4FileNode::Read(uint32_t offset, uint32_t size, uint8_t* buf) {
  size_t cnt = 0;
  auto ret = ext4_fread(m_file, buf, size, &cnt);

  if (ret != EOK) {
    return 0;
  }

  return cnt;
}

uint32_t Ext4FileNode::Write(uint32_t offset, uint32_t size, uint8_t* buf) {
  // TODO: Implement this function
  return 0;
}

bool Ext4FileNode::Seek(uint32_t offset) {
  auto ret = ext4_fseek(m_file, offset, SEEK_SET);

  return ret == EOK;
}

void Ext4FileNode::Close() {
  if (m_file) {
    ext4_fclose(m_file);

    delete m_file;
  }
}

}  // namespace fs