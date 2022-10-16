
#include <boot/toy_boot.h>

#include "disk/fat.h"
#include "elf.h"
#include "printf.h"
#include "screen/screen.h"
#include "x86/bios.h"

#define KERNEL_FILE_ADDR 0x10000
#define MAX_MEMORY_BLOCK 256

struct MemoryRegion g_mem_region[MAX_MEMORY_BLOCK];
uint32_t g_mem_region_count = 0;

typedef void (*KernelStart)(BootInfo *);

int detect_memory() {
  uint32_t next = 0;

  int ret = 0;

  do {
    ret = bios_memory_detect(g_mem_region + g_mem_region_count, &next);

    if (ret == -1) {
      return 1;
    }

    printf("memory at: %x | length: %x | type: %d \n",
           (uint32_t)g_mem_region[g_mem_region_count].base,
           (uint32_t)g_mem_region[g_mem_region_count].length,
           g_mem_region[g_mem_region_count].type);

    g_mem_region_count++;
  } while (next != 0);

  return 0;
}

void stage2_main(void *info, uint16_t boot_drive) {
  struct vbe_mode_info_structure *vbe_info =
      (struct vbe_mode_info_structure *)info;
  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  printf("Print In Protect Mode: fb addr is %x \n",
         ((struct vbe_mode_info_structure *)info)->framebuffer);

  // detect memory
  if (detect_memory()) {
    screen_print("memory detect failed!", 21, SCREEN_COLOR_RED);
    return;
  }

  if (fat_init(boot_drive)) {
    screen_print("fat init failed!", 16, SCREEN_COLOR_RED);
    return;
  }

  printf("\n begin load kernel file:\n");
  struct FAT_FILE *file = fat_kernel_file();

  if (!file) {
    printf("No kernel file !!\n");
    return;
  }

  if (fat_load_file(file, (uint32_t)KERNEL_FILE_ADDR)) {
    printf("Failed load kernel into memory!!");
    return;
  }

  uint32_t kernel_entry = elf_load((void *)KERNEL_FILE_ADDR);

  if (kernel_entry == 0) {
    printf("Failed load kernel elf file!!\n");
    return;
  }

  printf("kernel entry at: %x\n", kernel_entry);

  KernelStart entry = (KernelStart)kernel_entry;

  BootInfo boot_info;

  boot_info.frame_buffer.addr = vbe_info->framebuffer;
  boot_info.frame_buffer.width = vbe_info->width;
  boot_info.frame_buffer.height = vbe_info->height;
  boot_info.frame_buffer.pitch = vbe_info->pitch;
  boot_info.frame_buffer.bpp = vbe_info->bpp / 8;

  boot_info.memory_info = g_mem_region;
  boot_info.memory_info_count = g_mem_region_count;

  entry(&boot_info);

  return;
}
