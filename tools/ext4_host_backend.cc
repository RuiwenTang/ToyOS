#include "ext4_host_backend.hpp"

#include <ext4_mbr.h>
#include <ext4_mkfs.h>

#include <array>
#include <cstdio>
#include <filesystem>

constexpr uint64_t EXT4_FILEDEV_BSIZE = 512;

constexpr uint64_t SECTOR_COUNT = 0x80000;

uint8_t p_buffer[EXT4_FILEDEV_BSIZE];

extern "C" int mem_file_dev_open(struct ext4_blockdev* bdev);

extern "C" int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf,
                                  uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                                   uint64_t blk_id, uint32_t blk_cnt);

extern "C" int mem_file_dev_close(struct ext4_blockdev* bdev);

EXT4_BLOCKDEV_STATIC_INSTANCE(s_ram_block, EXT4_FILEDEV_BSIZE, SECTOR_COUNT,
                              mem_file_dev_open, mem_file_dev_bread,
                              mem_file_dev_bwrite, mem_file_dev_close, 0, 0);

Ext4File::Ext4File(std::string path) : m_file_path(std::move(path)) {}

Ext4File::~Ext4File() {
  if (m_file_stream.is_open()) {
    m_file_stream.flush();
    m_file_stream.close();
  }
}

bool Ext4File::IsValid() const { return m_file_stream.is_open(); }

bool Ext4File::Format(const std::string& type) {
  uint32_t fs_type = 0;

  if (type == "ext2") {
    fs_type = F_SET_EXT2;
  } else if (type == "ext3") {
    fs_type = F_SET_EXT3;
  } else if (type == "ext4") {
    fs_type = F_SET_EXT4;
  }

  if (fs_type == 0) {
    // unknown fs type
    return false;
  }

  if (!Init()) {
    return false;
  }

  if (!LoadPartionDev()) {
    return false;
  }

  ext4_fs fs{};

  ext4_mkfs_info info{
      .block_size = 1024,
      .journal = false,
  };

  auto r = ext4_mkfs(&fs, &m_sub_dev, &info, fs_type);

  return r == EOK;
}

bool Ext4File::Install(const std::string& src, const std::string& dst) {
  if (!Init()) {
    return false;
  }

  if (!LoadPartionDev()) {
    return false;
  }

  auto r = ext4_device_register(&m_sub_dev, "ext4_fs");

  if (r != EOK) {
    return false;
  }

  r = ext4_mount("ext4_fs", "/", false);

  if (r != EOK) {
    return false;
  }

  ext4_file file{};

  r = ext4_fopen(&file, dst.c_str(), "w");

  if (r != EOK) {
    return false;
  }

  std::FILE* src_file = std::fopen(src.c_str(), "rb");
  if (src_file == nullptr) {
    return false;
  }

  std::array<char, 512> buffer{};

  int32_t in_bytes = std::fread(buffer.data(), 1, 512, src_file);

  while (in_bytes > 0) {
    r = ext4_fwrite(&file, buffer.data(), in_bytes, nullptr);

    if (r != EOK) {
      return false;
    }

    in_bytes = std::fread(buffer.data(), 1, 512, src_file);
  }

  r = ext4_fclose(&file);

  if (r != EOK) {
    return false;
  }

  std::fclose(src_file);

  return true;
}

void Ext4File::Seekg(uint64_t offset) {
  m_file_stream.seekg(offset, std::fstream::beg);
}

void Ext4File::Read(char* buf, uint64_t size) { m_file_stream.read(buf, size); }

void Ext4File::Seekp(uint64_t offset) {
  m_file_stream.seekp(offset, std::fstream::beg);
}

void Ext4File::Write(char* buf, uint64_t size) {
  m_file_stream.write(buf, size);
}

bool Ext4File::Init() {
  if (m_file_path.empty()) {
    return false;
  }

  m_file_stream.open(
      m_file_path, std::fstream::in | std::fstream::out | std::fstream::binary);

  if (!m_file_stream.is_open()) {
    std::ofstream out_file(m_file_path, std::ios::binary);

    out_file.seekp(SECTOR_COUNT * EXT4_FILEDEV_BSIZE - 1);
    char a = '\0';
    out_file.write(&a, 1);
    out_file.close();
  }

  if (!m_file_stream.is_open()) {
    m_file_stream.open(m_file_path, std::fstream::in | std::fstream::out |
                                        std::fstream::binary);
  }

  auto size = m_file_stream.tellg();

  m_file_stream.seekg(0, std::ios::end);

  auto file_size = m_file_stream.tellg() - size;

  if (file_size != SECTOR_COUNT * EXT4_FILEDEV_BSIZE) {
    return false;
  }

  // Init ram block dev
  s_ram_block.bdif->p_user = this;

  return true;
}

bool Ext4File::LoadPartionDev() {
  ext4_mbr_bdevs devs{};
  auto r = ext4_mbr_scan(&s_ram_block, &devs);

  if (r == EOK && devs.partitions[0].bdif) {
    m_sub_dev = devs.partitions[0];

    return true;
  }

  // need to partion first
  struct ext4_mbr_parts parts {};
  parts.division[0] = 100;
  parts.division[1] = 0;
  parts.division[2] = 0;
  parts.division[3] = 0;

  r = ext4_mbr_write(&s_ram_block, &parts, 0);

  if (r != EOK) {
    return false;
  }

  r = ext4_mbr_scan(&s_ram_block, &devs);

  if (r != EOK) {
    return false;
  }

  m_sub_dev = devs.partitions[0];

  return true;
}

extern "C" {

int mem_file_dev_open(struct ext4_blockdev* bdev) {
  auto* file = reinterpret_cast<Ext4File*>(bdev->bdif->p_user);

  if (file->IsValid()) {
    return EOK;
  }

  return ENODEV;
}

int mem_file_dev_bread(struct ext4_blockdev* bdev, void* buf, uint64_t blk_id,
                       uint32_t blk_cnt) {
  auto* file = reinterpret_cast<Ext4File*>(bdev->bdif->p_user);

  if (!file->IsValid()) {
    return ENODEV;
  }

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  file->Seekg(offset);

  file->Read((char*)buf, blk_cnt * EXT4_FILEDEV_BSIZE);

  return EOK;
}

int mem_file_dev_bwrite(struct ext4_blockdev* bdev, const void* buf,
                        uint64_t blk_id, uint32_t blk_cnt) {
  auto* file = reinterpret_cast<Ext4File*>(bdev->bdif->p_user);

  if (!file->IsValid()) {
    return ENODEV;
  }

  auto offset = blk_id * EXT4_FILEDEV_BSIZE;

  file->Seekp(offset);

  file->Write((char*)buf, blk_cnt * EXT4_FILEDEV_BSIZE);

  return EOK;
}

int mem_file_dev_close(struct ext4_blockdev* bdev) { return EOK; }
}
