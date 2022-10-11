#include "disk/fat.h"
#include "disk/mbr.h"
#include "screen/screen.h"
#include "x86/bios.h"

uint8_t fat_root[512];

uint32_t disk_lba_start;
uint32_t disk_lba_count;

struct BPB *fat_bpb = 0;

int fat_init(uint16_t boot_drive) {
  struct PTE *pte = mbr_read_pte();

  // for now hard code partion index
  disk_lba_start = pte->lba_start;
  disk_lba_count = pte->lba_count;

  if (disk_lba_count == 0) {
    return 1;
  }

  if (bios_disk_read(boot_drive, disk_lba_start, fat_root)) {
    return 1;
  }

  fat_bpb = (struct BPB *)fat_root;

  screen_print((char *)fat_bpb->oem_name, 8, SCREEN_COLOR_WHITE);

  return 0;
}