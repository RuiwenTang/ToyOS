
#include "screen/screen.h"
#include "x86/bios.h"

char buffer[512];

void stage2_main(void *info, uint16_t boot_drive) {

  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  char msg[] = "Print In Protect Mode";

  screen_print(msg, sizeof(msg), SCREEN_COLOR_RED);

  int ret = bios_disk_read((uint8_t)boot_drive, 63, buffer);

  if (ret != 0) {
    screen_print("read disk failed", 16, SCREEN_COLOR_WHITE);
  }

  char *oem_str = buffer + 3;
  screen_print(oem_str, 8, SCREEN_COLOR_WHITE);

  return;
}
