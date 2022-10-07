
#include "diskio.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

struct RamFloppy {
  std::vector<BYTE> disk;

  RamFloppy() : disk(512 * 2880) {}
};

RamFloppy *g_floppy = nullptr;

extern "C" {

// PARTITION VolToPart[FF_VOLUMES] = {
//     PARTITION{0, 1}, /* "0:" ==> 1st partition in PD#0 */
//     {0, 2},          /* "1:" ==> 2nd partition in PD#0 */
//     {1, 0}           /* "2:" ==> PD#1 as removable drive */
// };

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
    *(LBA_t *)buff = 2880;
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
  opt.au_size = 512;
  opt.n_root = 0;

  res = f_mount(&fs, "", 0);

  // LBA_t plist[] = {90, 10, 0};

  // res = f_fdisk(0, plist, work);

  res = f_mkfs("", 0, work, sizeof work);

  res = f_open(&fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS);

  if (res == FR_OK) {
    res = f_write(&fil, "It works!\r\n", 11, &bw);

    res = f_close(&fil);
  }

  if (res == FR_OK) {
    std::ofstream out_fs("floppy.img", std::ios::out | std::ios::binary);

    g_floppy->disk[0x1BE] |= 0x80;

    out_fs.write((char *)g_floppy->disk.data(), g_floppy->disk.size());

    out_fs.close();
  }

  return 0;
}
