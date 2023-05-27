
#include <ext4.h>
#include <ext4_fs.h>
#include <ext4_mkfs.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

#define EXT4_FILEDEV_BSIZE 512

#define SECTOR_COUNT 0x80000

extern "C" int mem_file_dev_open(struct ext4_blockdev* bdev);

extern "C" int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf,
                                  uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                                   uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_close(struct ext4_blockdev* bdev);

extern "C" {

uint8_t p_buffer[EXT4_FILEDEV_BSIZE];

struct ext4_blockdev_iface s_i_face {
  .open = mem_file_dev_open, .bread = mem_file_dev_bread,
  .bwrite = mem_file_dev_bwrite, .close = mem_file_dev_close, .lock = nullptr,
  .unlock = nullptr, .ph_bsize = EXT4_FILEDEV_BSIZE, .ph_bbuf = p_buffer,
};
}

struct RamBlockDev {
  struct ext4_blockdev super;
  const char* path;
  std::FILE* file;

  RamBlockDev(const char* path) : path(path), file(nullptr) {
    super.bdif = &s_i_face;
  }

  ~RamBlockDev() {
    if (file) {
      std::fclose(file);
      file = nullptr;
    }
  }
};

extern "C" {

int mem_file_dev_open(struct ext4_blockdev* bdev) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  dev->super.part_offset = 0;
  dev->super.part_size = SECTOR_COUNT * EXT4_FILEDEV_BSIZE;
  dev->super.bdif->ph_bcnt = SECTOR_COUNT;

  dev->file = std::fopen(dev->path, "w+b");

  if (!dev->file) {
    return ENOENT;
  }

  std::fseek(dev->file, SECTOR_COUNT * EXT4_FILEDEV_BSIZE, SEEK_SET);
  std::fseek(dev->file, 0, SEEK_SET);

  return EOK;
}

int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf, uint64_t blk_id,
                       uint32_t blk_cnt) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  if (!dev->file) {
    return ENFILE;
  }

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  std::fseek(dev->file, offset, SEEK_SET);

  std::fread(buf, blk_cnt * EXT4_FILEDEV_BSIZE, 1, dev->file);

  return EOK;
}

int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                        uint64_t blk_id, uint32_t blk_cnt) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  if (!dev->file) {
    return ENFILE;
  }

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  std::fseek(dev->file, offset, SEEK_SET);

  std::fwrite(buf, blk_cnt * EXT4_FILEDEV_BSIZE, 1, dev->file);

  return EOK;
}

int mem_file_dev_close(struct ext4_blockdev* bdev) {
  auto dev = reinterpret_cast<RamBlockDev*>(bdev);

  if (!dev->file) {
    return ENFILE;
  }

  std::fclose(dev->file);

  dev->file = nullptr;

  return EOK;
}
}

int main(int argc, const char** argv) {
  RamBlockDev mem_dev("demo_ext2.img");

  ext4_device_register(reinterpret_cast<struct ext4_blockdev*>(&mem_dev),
                       "ext2_fs");

  ext4_fs fs{};

  ext4_mkfs_info info{
      .block_size = 1024,
      .journal = false,
  };

  auto r = ext4_mkfs(&fs, reinterpret_cast<struct ext4_blockdev*>(&mem_dev),
                     &info, F_SET_EXT2);

  if (r != EOK) {
    std::cerr << "ext4 mkfs : rc = " << r << std::endl;
    return -1;
  }

  r = ext4_mount("ext2_fs", "/", false);

  if (r != EOK) {
    std::cerr << "ext4_mount : rc = " << r << std::endl;

    return -1;
  }

  return 0;
}