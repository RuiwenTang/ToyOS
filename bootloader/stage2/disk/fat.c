#include "disk/fat.h"

#include <stdbool.h>
#include <stddef.h>

#include "disk/mbr.h"
#include "printf.h"
#include "screen/screen.h"
#include "x86/bios.h"

#define MEM_DIR_BASE 0x70000

static bool _memcmp(uint8_t *s1, uint8_t *s2, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if (s1[i] != s2[i]) {
      return false;
    }
  }

  return true;
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

struct FAT_FILE {
  struct DIR_ENTRY fat_entry;
  // FIXME cluster is 2 sector
  uint32_t fist_cluster;
  uint32_t total_cluster;
};

uint8_t fat_root[512];

uint8_t read_buf[512];

uint8_t g_fat_tab[512];
uint32_t g_fat_tab_index = 0;

uint32_t disk_lba_start;
uint32_t disk_lba_count;

struct FAT_INFO g_fat_info;

struct BPB *fat_bpb = 0;

struct DIR_ENTRY *g_root_dir = (struct DIR_ENTRY *)MEM_DIR_BASE;

// private function
void fat_dump();

uint16_t read_fat_table(uint32_t cluster);

uint32_t total_cluster(uint32_t fist_cluster);

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

  // read first fat sector into fat table

  if (bios_disk_read(g_fat_info.drive_num,
                     disk_lba_start + g_fat_info.first_fat_sector, g_fat_tab)) {
    return 1;
  }

  fat_dump();

  // try read first
  // basic the first directory contains the kernel file
  if (bios_disk_read(g_fat_info.drive_num,
                     disk_lba_start + g_fat_info.first_data_sector -
                         g_fat_info.root_dir_sectors,
                     (void *)MEM_DIR_BASE)) {
    return 1;
  }

  if (!g_root_dir->name[0]) {
    return 1;
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

uint32_t first_sector(uint32_t cluster) {
  return (cluster - 2) * fat_bpb->sectors_per_cluster +
         g_fat_info.first_data_sector + disk_lba_start;
}

// TODO support LFN entry

struct DIR_ENTRY *boot_dir = NULL;

struct FAT_FILE kernel_file;

struct FAT_FILE *fat_kernel_file() {
  for (uint32_t i = 0; i < g_fat_info.root_dir_sectors; i++) {
    struct DIR_ENTRY *p_dir = g_root_dir + i * 512;

    for (uint32_t j = 0; j < 512 / 32; j++) {
      if (_memcmp(p_dir[j].name, "BOOT       ", 11)) {
        // found boot dir
        boot_dir = p_dir + j;
        break;
      }
    }

    if (boot_dir) {
      break;
    }
  }

  if (!boot_dir || ((boot_dir->attribute & FAT_ATTR_DIRECTORY) == 0)) {
    return NULL;
  }

  uint32_t cluster =
      boot_dir->first_cluster_high << 8 | boot_dir->first_cluster_low;
  uint32_t start_sector = first_sector(cluster);
  printf("BOOT dir found, start cluster = %d at sector %d \n", cluster,
         start_sector);

  if (bios_disk_read(g_fat_info.drive_num, start_sector, read_buf)) {
    // read dir content failed
    return NULL;
  }

  struct DIR_ENTRY *boot_content = (struct DIR_ENTRY *)read_buf;

  bool found_kernel = false;

  // find KERNEL  SYS
  for (uint32_t j = 0; j < 512 / 32; j++) {
    if (_memcmp(boot_content[j].name, "KERNEL  SYS", 11)) {
      // find kernel file

      found_kernel = true;

      kernel_file.fat_entry.attribute = boot_content[j].attribute;
      kernel_file.fat_entry.create_time = boot_content[j].create_time;
      kernel_file.fat_entry.create_data = boot_content[j].create_data;
      kernel_file.fat_entry.access_time = boot_content[j].access_time;
      kernel_file.fat_entry.first_cluster_high =
          boot_content[j].first_cluster_high;
      kernel_file.fat_entry.modified_time = boot_content[j].modified_time;
      kernel_file.fat_entry.modified_data = boot_content[j].modified_data;
      kernel_file.fat_entry.first_cluster_low =
          boot_content[j].first_cluster_low;
      kernel_file.fat_entry.size = boot_content[j].size;

      break;
    }
  }

  if (!found_kernel) {
    return NULL;
  }

  uint32_t kernel_cluster = kernel_file.fat_entry.first_cluster_high << 8 |
                            kernel_file.fat_entry.first_cluster_low;

  printf("kernel file found: first cluster = %d | file size: %d bytes\n",
         kernel_cluster, kernel_file.fat_entry.size);

  uint32_t cluster_count = total_cluster(kernel_cluster);

  printf("total kernel cluster is %d \n", cluster_count);

  if (kernel_file.fat_entry.size > 0x60000) {
    printf("kernel file is too large!!\n");
    return NULL;
  }

  kernel_file.fist_cluster = kernel_cluster;
  kernel_file.total_cluster = cluster_count;

  return &kernel_file;
}

uint16_t read_fat_table(uint32_t cluster) {
  uint32_t fat_offset = cluster * 2;

  if (fat_offset / 512 > g_fat_tab_index) {
    // need to read next fat
    printf("need read next fat table\n");
  }

  uint32_t fat_sector = g_fat_info.first_fat_sector + (fat_offset / 512);
  uint32_t ent_offset = fat_offset % 512;

  uint16_t table_value = *(uint16_t *)&g_fat_tab[ent_offset];

  return table_value;
}

uint32_t total_cluster(uint32_t fist_cluster) {
  uint32_t size = 1;

  uint16_t value = read_fat_table(fist_cluster);

  while (value < 0xFFF8) {
    if (value == 0xFFF7) {
      // bad sector and kernel is broken
      return 0;
    }

    size++;
    value = read_fat_table((uint32_t)value);
  }

  return size;
}

int fat_load_file(struct FAT_FILE *file, uint32_t addr) {
  uint32_t curr = addr;

  uint32_t curr_cluster = file->fist_cluster;
  uint32_t i_cluster = 1;

  do {
    uint32_t f_sector = first_sector(curr_cluster);

    for (int i = 0; i < fat_bpb->sectors_per_cluster; i++) {
      if (bios_disk_read(g_fat_info.drive_num, f_sector + i, (void *)curr)) {
        return 1;
      }
      curr += 512;
    }

    uint16_t value = read_fat_table(curr_cluster);

    if (value == 0xFFF8) {
      // end cluster
      break;
    }
    if (value == 0xFFF7) {
      // bad sector
      return 1;
    }

    curr_cluster = (uint32_t)value;
    i_cluster++;
  } while (i_cluster < file->total_cluster);

  return 0;
}
