#ifndef BOOT_BOOT_H
#define BOOT_BOOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;
} Framebuffer;

enum MEM_TYPE {
  USABLE = 1,
  RESERVED = 2,
  ACPI_RECLAI = 3,
  ACPI_NVS = 4,
  BAD_MEMORY = 5,
};

struct MemoryRegion {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t acpi;
};

typedef struct MemoryRegion MemoryRegionT;

typedef struct {
  MemoryRegionT* memory_info;
  uint32_t memory_info_count;
  Framebuffer frame_buffer;
} BootInfo;

#ifdef __cplusplus
}
#endif

#endif  // BOOT_BOOT_H
