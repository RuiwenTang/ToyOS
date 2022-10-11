
#include "disk/fat.h"
#include "screen/screen.h"

void stage2_main(void *info, uint16_t boot_drive) {

  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  char msg[] = "Print In Protect Mode\n";

  screen_print(msg, sizeof(msg), SCREEN_COLOR_RED);

  if (fat_init(boot_drive)) {
    screen_print("fat init failed!", 16, SCREEN_COLOR_RED);
    return;
  }

  return;
}
