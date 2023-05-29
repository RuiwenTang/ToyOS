#include "ext4_host_backend.hpp"

#define EXT4_FILEDEV_BSIZE 512

#define SECTOR_COUNT 0x80000

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
    m_file_stream.close();
  }
}

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

  return true;
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
    out_file.write("1", 1);
    out_file.close();
  }

  m_file_stream.open(
      m_file_path, std::fstream::in | std::fstream::out | std::fstream::binary);

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