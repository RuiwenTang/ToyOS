#ifndef BOOT_DISK_MBR_H
#define BOOT_DISK_MBR_H

#include <stdint.h>

#define PARTION_COUNT 4

// Partion table entry
// https://wiki.osdev.org/MBR_(x86)
struct __attribute__((packed)) PTE {
  uint8_t attribute;    // offset 0, bit 7 set = active or bootable
  uint8_t chs_start[3]; // CHS address of partion start
  uint8_t type;         // Partion type
  uint8_t chs_end[3];   // chs address of last partion sector
  uint32_t lba_start;   // LBA of partion start
  uint32_t lba_count;   // number of sectors in partion
};

struct PTE *mbr_read_pte();

#endif // BOOT_DISK_MBR_H