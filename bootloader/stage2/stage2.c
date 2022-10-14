
#include "disk/fat.h"
#include "printf.h"
#include "screen/screen.h"

void stage2_main(void *info, uint16_t boot_drive) {

  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  printf("Print In Protect Mode: fb addr is %x \n",
         ((struct vbe_mode_info_structure *)info)->framebuffer);

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

  return;
}
