
#include <boot/toy_boot.h>

void kernel_main(BootInfo *boot_info) {
  uint8_t *addr = (uint8_t *)boot_info->frame_buffer.addr;

  for (uint32_t x = 0; x < boot_info->frame_buffer.width; x++) {
    for (uint32_t y = 0; y < boot_info->frame_buffer.height; y++) {
      uint8_t *pix = addr + y * boot_info->frame_buffer.pitch +
                     x * boot_info->frame_buffer.bpp;

      for (uint32_t i = 0; i < boot_info->frame_buffer.bpp; i++) {
        pix[i] = 0xff;
      }
    }
  }

  int *p = (int *)0x100000;

  *p = 0xAABBCCDD;
}