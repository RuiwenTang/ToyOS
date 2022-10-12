#include "disk/fat.h"
#include "disk/mbr.h"
#include "printf.h"
#include "x86/bios.h"
#include <stdbool.h>

uint8_t fat_root[512];

uint32_t disk_lba_start;
uint32_t disk_lba_count;
bool fat_32 = false;

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

  fat_32 = fat_bpb->total_sectors == 0;

  printf("disk oem name: %s\n", fat_bpb->oem_name);
  printf("partion start lba: %d | sector count: %d\n", fat_bpb->hidden_sectors,
         fat_32 ? fat_bpb->large_sector_count : fat_bpb->total_sectors);
  printf("number of fat: %d | root entry count: %d |", fat_bpb->fat_count,
         fat_bpb->dir_entry_count);
  printf("bytes per sector: %d | sector per cluster: %d |",
         fat_bpb->bytes_per_sector, fat_bpb->sectors_per_cluster);
  printf("reserved sectors: %d\n", fat_bpb->reserved_sectors);

  printf("sector per fat: %d", (uint32_t)fat_bpb->sector_per_fat);

  return 0;
}