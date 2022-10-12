#include "disk/fat.h"
#include "disk/mbr.h"
#include "printf.h"
#include "screen/screen.h"
#include "x86/bios.h"
#include <stdbool.h>

#define MEM_DIR_BASE 0x70000

static void _memcpy(uint8_t *dst, uint8_t *str, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    dst[i] = str[i];
  }
}

struct FAT_INFO {
  enum FAT_TYPE fat_type;
  uint16_t drive_num;
  uint32_t total_sectors;
  uint32_t fat_size;
  uint32_t root_dir_sectors;
  uint32_t first_data_sector;
  uint32_t first_fat_sector;
  uint32_t data_sectors;
  uint32_t total_clusters;
};

uint8_t fat_root[512];

uint8_t read_buf[512];

uint32_t disk_lba_start;
uint32_t disk_lba_count;

struct FAT_INFO g_fat_info;

struct BPB *fat_bpb = 0;

void fat_dump();

int fat_init(uint16_t boot_drive) {
  struct PTE *pte = mbr_read_pte();

  // for now hard code partion index
  disk_lba_start = pte->lba_start;
  disk_lba_count = pte->lba_count;

  if (disk_lba_count == 0) {
    return 1;
  }

  if (bios_disk_read(boot_drive, disk_lba_start, (void *)fat_root)) {
    return 1;
  }

  fat_bpb = (struct BPB *)fat_root;

  g_fat_info.drive_num = boot_drive;

  g_fat_info.total_sectors = fat_bpb->total_sectors == 0
                                 ? fat_bpb->large_sector_count
                                 : fat_bpb->total_sectors;

  g_fat_info.fat_size = fat_bpb->sector_per_fat == 0
                            ? fat_bpb->ebr_32.sector_per_fat
                            : fat_bpb->sector_per_fat;

  // FAT32 this is zero
  g_fat_info.root_dir_sectors =
      ((fat_bpb->dir_entry_count * 32) + (fat_bpb->bytes_per_sector - 1)) /
      fat_bpb->bytes_per_sector;

  g_fat_info.first_data_sector = fat_bpb->reserved_sectors +
                                 (fat_bpb->fat_count * g_fat_info.fat_size) +
                                 g_fat_info.root_dir_sectors;

  g_fat_info.first_fat_sector = fat_bpb->reserved_sectors;

  g_fat_info.data_sectors =
      g_fat_info.total_sectors -
      (fat_bpb->reserved_sectors + (fat_bpb->fat_count * g_fat_info.fat_size) +
       g_fat_info.root_dir_sectors);

  g_fat_info.total_clusters =
      g_fat_info.data_sectors / fat_bpb->sectors_per_cluster;

  // check fat type
  if (fat_bpb->bytes_per_sector == 0) {
    g_fat_info.fat_type = FAT_EXFAT;
  } else if (g_fat_info.total_clusters < 4085) {
    g_fat_info.fat_type = FAT_FAT12;
  } else if (g_fat_info.total_clusters < 65525) {
    g_fat_info.fat_type = FAT_FAT16;
  } else {
    g_fat_info.fat_type = FAT_FAT32;
  }

  fat_dump();

  // try read first

  if (bios_disk_read(g_fat_info.drive_num,
                     disk_lba_start + g_fat_info.first_data_sector -
                         g_fat_info.root_dir_sectors,
                     (void *)read_buf)) {
    return 1;
  }

  _memcpy((uint8_t *)MEM_DIR_BASE, read_buf, 512);

  struct DIR_ENTRY *p_dir = (struct DIR_ENTRY *)MEM_DIR_BASE;

  if (p_dir->name[0]) {
    screen_print(p_dir->name, 11, SCREEN_COLOR_RED);

    printf(" | file attr = %x", (uint32_t)p_dir->attribute);
  }

  return 0;
}

void fat_dump() {
  printf("disk oem name: %s\n", fat_bpb->oem_name);

  switch (g_fat_info.fat_type) {
  case FAT_EXFAT:
    printf("fat type: EXFAT\n");
    break;
  case FAT_FAT12:
    printf("fat type: FAT12\n");
    break;
  case FAT_FAT16:
    printf("fat type: FAT16\n");
    break;
  case FAT_FAT32:
    printf("fat type: FAT32\n");
    break;
  }

  printf("partion start lba: %d | sector count: %d\n", disk_lba_start,
         disk_lba_count);

  printf("fat size: %d | root dir sectors: %d |", g_fat_info.fat_size,
         g_fat_info.root_dir_sectors);

  printf("total sectors: %d | total clusters: %d\n", g_fat_info.total_sectors,
         g_fat_info.total_clusters);

  printf("first fat sector: %d | first data sector: %d",
         g_fat_info.first_fat_sector, g_fat_info.first_data_sector);
}