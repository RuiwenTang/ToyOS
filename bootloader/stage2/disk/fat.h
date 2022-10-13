#ifndef BOOT_DISK_FAT_H
#define BOOT_DISK_FAT_H

#include <stdint.h>

// https://wiki.osdev.org/FAT

struct __attribute__((packed)) EXTBR {
  uint8_t drive_number;
  uint8_t reserved;
  uint8_t signature;
  uint32_t volume_id;
  uint8_t volume_label[11];
  uint8_t system_id[8];
};

struct __attribute__((packed)) FAT32_EXTBR {
  uint32_t sector_per_fat;
  uint16_t flags;
  uint16_t fat_version;
  uint32_t root_director_cluster;
  uint16_t fs_info_sector;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  struct EXTBR ebr;
};

// BIOS Parameter Block
struct __attribute__((packed)) BPB {
  uint8_t boot_jump[3];        // ignored
  uint8_t oem_name[8];         // ignored
  uint16_t bytes_per_sector;   //
  uint8_t sectors_per_cluster; //
  uint16_t reserved_sectors;   // Number of reserved sectors. The boot record
                               // sectors are included in this value.
  uint8_t fat_count;           // should be 2
  uint16_t dir_entry_count;    // number of root entry
  uint16_t total_sectors;      // 0 means large lba
  uint8_t media_type;          // ignored
  uint16_t sector_per_fat;     // FAT12/FAT16 only, ignored
  uint16_t sector_per_track;   //
  uint16_t heads;              //
  uint32_t hidden_sectors;     // LBA of begining of the partion
  uint32_t large_sector_count;
  union {
    struct EXTBR ebr_16;
    struct FAT32_EXTBR ebr_32;
  };
  // ...
  // boot code
};

enum FAT_ATTR {
  FAT_ATTR_READ_ONLY = 0x01,
  FAT_ATTR_HIDDEN = 0x02,
  FAT_ATTR_SYSTEM = 0x04,
  FAT_ATTR_VOLUME_ID = 0x08,
  FAT_ATTR_DIRECTORY = 0x10,
  FAT_ATTR_ARCHIVE = 0x20,
  FAT_ATTR_LFN = FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM |
                 FAT_ATTR_VOLUME_ID
};

enum FAT_TYPE {
  FAT_FAT12 = 1,
  FAT_FAT16,
  FAT_FAT32,
  FAT_EXFAT,
};

struct __attribute__((packed)) DIR_ENTRY {
  uint8_t name[11];    // 8.3 name,
  uint8_t attribute;   // READ_ONLY=0x01 HIDDEN=0x02 SYSTEM=0x04 VOLUME_ID=0x08
                       // DIRECTORY=0x10 ARCHIVE=0x20
                       // LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID (means long file
                       // name entry)
  uint8_t reserved;    // ignored
  uint8_t time_tenths; // ignored
  uint16_t create_time; // Hour    5 bits
                        // Minutes 6 bits
                        // seconds 5 bits
  uint16_t create_data; // year  7 bits
                        // month 4 bits
                        // day   5 bits
  uint16_t access_time; // same as data
  uint16_t first_cluster_high;
  uint16_t modified_time;
  uint16_t modified_data;
  uint16_t first_cluster_low;
  uint32_t size; // size in bytes
};

struct __attribute__((packed)) DIR_LFN_ENTRY {
  uint8_t order;
  int16_t chars1[5];       // first 5 charaters (2 bytes char)
  uint8_t attributes;      // always equal 0x0F
  uint8_t long_entry_type; // 0 means name entry
  uint8_t check_sum;
  uint16_t chars2[6];
  uint16_t always_zero;
  uint16_t chars3[2];
};

struct FAT_FILE;

int fat_init(uint16_t boot_drive);

// hard code kernel file location and file name
// boot/kernel.sys
// BOOT
// KERNEL  SYS
struct FAT_FILE *fat_kernel_file();

#endif // BOOT_DISK_FAT_H