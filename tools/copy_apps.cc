
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "diskio.h"

#define SECTOR_COUNT 0x80000

const char *g_path = nullptr;

struct RamFloppy {
  std::FILE *file;

  RamFloppy(const char *path) : file(std::fopen(path, "r+")) {}
};

RamFloppy *g_floppy = nullptr;

extern "C" {

/**
 * Only support ram disk in memory
 */
DSTATUS RAM_disk_initialize() {
  if (!g_floppy) {
    g_floppy = new RamFloppy(g_path);
  }

  return 0;
}

DSTATUS RAM_disk_read(BYTE *buf, LBA_t sector, UINT count) {
  if (!g_floppy) {
    return STA_NOINIT;
  }

  std::fseek(g_floppy->file, sector * 512, SEEK_SET);

  std::fread(buf, count * 512, 1, g_floppy->file);

  return 0;
}

DSTATUS RAM_disk_write(const BYTE *buf, LBA_t sector, UINT count) {
  if (!g_floppy) {
    return STA_NOINIT;
  }

  std::fseek(g_floppy->file, sector * 512, SEEK_SET);
  std::fwrite(buf, count * 512, 1, g_floppy->file);

  return 0;
}

DSTATUS RAM_disk_ioctl(BYTE cmd, void *buff) {
  if (cmd == GET_SECTOR_COUNT) {
    *(LBA_t *)buff = SECTOR_COUNT;
  }

  return 0;
}
}

int main(int argc, const char **argv) {
  FATFS fs;    /* Filesystem object */
  FIL fil;     /* File object */
  FRESULT res; /* API result code */
  UINT bw;     /* Bytes written */

  MKFS_PARM opt;
  opt.fmt = FM_ANY;
  opt.n_fat = 2;
  opt.align = 512;
  opt.au_size = 0;
  opt.n_root = 0;

  g_path = argv[1];

  res = f_mount(&fs, "", 0);

  res = f_mkdir("system");

  if (res != FR_OK) {
    return -1;
  }

  {
    std::ifstream init_stream(argv[2], std::ios::in | std::ios::binary);
    std::vector<BYTE> init_bin{(std::istreambuf_iterator<char>(init_stream)),
                               std::istreambuf_iterator<char>()};

    std::cout << "size of kernel file: " << init_bin.size() << std::endl;

    res = f_open(&fil, "system/init", FA_WRITE | FA_CREATE_ALWAYS);

    res = f_write(&fil, (const void *)init_bin.data(), (UINT)init_bin.size(),
                  &bw);

    res = f_close(&fil);
  }

  if (res == FR_OK) {
    std::fclose(g_floppy->file);
  }

  return res;
}
