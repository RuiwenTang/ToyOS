
#include "disk/fat.h"
#include "printf.h"
#include "screen/screen.h"

void stage2_main(void *info, uint16_t boot_drive) {

  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  printf("Print In Protect Mode\n");

  if (fat_init(boot_drive)) {
    screen_print("fat init failed!", 16, SCREEN_COLOR_RED);
    return;
  }

  return;
}
