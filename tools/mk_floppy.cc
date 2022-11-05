
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "diskio.h"

#define SECTOR_COUNT 0x80000

struct RamFloppy {
  std::vector<BYTE> disk;

  RamFloppy() : disk(512 * SECTOR_COUNT) {}
};

RamFloppy *g_floppy = nullptr;

extern "C" {

// PARTITION VolToPart[FF_VOLUMES] = {
//     PARTITION{0, 1}, /* "0:" ==> 1st partition in PD#0 */
//     {0, 2},          /* "1:" ==> 2nd partition in PD#0 */
//     {1, 0}           /* "2:" ==> PD#1 as removable drive */
// };

#pragma pack(push, 1)
struct PTE {
  BYTE dr_attr;
  BYTE chs_start[3];
  BYTE partion_type;
  BYTE chs_end[3];
  unsigned int lba_start;
  unsigned int n_sec;
};
#pragma pack(pop)

/**
 * Only support ram disk in memory
 */
DSTATUS RAM_disk_initialize() {
  if (!g_floppy) {
    g_floppy = new RamFloppy;
  }

  return 0;
}

DSTATUS RAM_disk_read(BYTE *buf, LBA_t sector, UINT count) {
  if (!g_floppy) {
    return STA_NOINIT;
  }

  BYTE *p = g_floppy->disk.data() + sector * 512;

  std::memcpy(buf, p, count * 512);

  return 0;
}

DSTATUS RAM_disk_write(const BYTE *buf, LBA_t sector, UINT count) {
  if (!g_floppy) {
    return STA_NOINIT;
  }

  BYTE *p = g_floppy->disk.data() + sector * 512;

  std::memcpy(p, buf, count * 512);

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
  FATFS fs;             /* Filesystem object */
  FIL fil;              /* File object */
  FRESULT res;          /* API result code */
  UINT bw;              /* Bytes written */
  BYTE work[FF_MAX_SS]; /* Work area (larger is better for processing time) */

  MKFS_PARM opt;
  opt.fmt = FM_ANY;
  opt.n_fat = 2;
  opt.align = 512;
  opt.au_size = 0;
  opt.n_root = 0;

  res = f_mount(&fs, "", 0);

  // LBA_t plist[] = {90, 10, 0};

  // res = f_fdisk(0, plist, work);

  res = f_mkfs("", &opt, work, sizeof work);

  res = f_open(&fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS);

  if (res == FR_OK) {
    res = f_write(&fil, "It works!\r\n", 11, &bw);

    res = f_close(&fil);
  }

  res = f_mkdir("boot");

  std::cout << "limine stage2 file: " << argv[2] << std::endl;

  {
    std::ifstream stage2_stream(argv[2], std::ios::in | std::ios::binary);

    std::vector<BYTE> stage2_bin{
        (std::istreambuf_iterator<char>(stage2_stream)),
        std::istreambuf_iterator<char>()};

    std::cout << "size of limine stage2 file: " << stage2_bin.size()
              << std::endl;

    res = f_open(&fil, "boot/limine.sys", FA_WRITE | FA_CREATE_ALWAYS);

    if (res != FR_OK) {
      std::cerr << "failed create boot/limine.sys err: " << res << std::endl;
      return -1;
    }

    res = f_write(&fil, stage2_bin.data(), stage2_bin.size(), &bw);

    if (res != FR_OK) {
      std::cerr << "failed write to boot/limine.sys err: " << res << std::endl;
      return -1;
    }

    res = f_close(&fil);
  }

  std::cout << "limine.cfg" << argv[3] << std::endl;
  {
    std::ifstream limine_cfg_stream(argv[3], std::ios::in | std::ios::binary);

    std::vector<BYTE> limine_cfg_bin{
        (std::istreambuf_iterator<char>(limine_cfg_stream)),
        std::istreambuf_iterator<char>()};

    std::cout << "size of limine.cfg file: " << limine_cfg_bin.size()
              << std::endl;

    res = f_open(&fil, "boot/limine.cfg", FA_WRITE | FA_CREATE_ALWAYS);

    if (res != FR_OK) {
      return -1;
    }

    res = f_write(&fil, limine_cfg_bin.data(), limine_cfg_bin.size(), &bw);

    if (res != FR_OK) {
      return -1;
    }

    f_close(&fil);
  }

  std::cout << "kernel file:" << argv[4] << std::endl;
  {
    std::ifstream kernel_stream(argv[4], std::ios::in | std::ios::binary);
    std::vector<BYTE> kernel_bin{
        (std::istreambuf_iterator<char>(kernel_stream)),
        std::istreambuf_iterator<char>()};

    std::cout << "size of kernel file: " << kernel_bin.size() << std::endl;

    res = f_open(&fil, "boot/kernel.sys", FA_WRITE | FA_CREATE_ALWAYS);

    res = f_write(&fil, (const void *)kernel_bin.data(),
                  (UINT)kernel_bin.size(), &bw);

    res = f_close(&fil);
  }

  if (res == FR_OK) {
    std::ofstream out_fs(argv[1], std::ios::out | std::ios::binary);

    out_fs.write((char *)g_floppy->disk.data(), g_floppy->disk.size());

    out_fs.close();
  }

  std::cout << "sizeof PTE " << sizeof(struct PTE) << std::endl;

  struct PTE *p_pte = (struct PTE *)(g_floppy->disk.data() + 446);

  std::cout << "start lba: " << p_pte->lba_start << std::endl;
  std::cout << "count sector: " << p_pte->n_sec << std::endl;

  return res;
}
