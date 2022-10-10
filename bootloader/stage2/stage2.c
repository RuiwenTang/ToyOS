

#include "screen/screen.h"

void stage2_main(void *info) {

  screen_init((struct vbe_mode_info_structure *)info);
  screen_clear();

  char msg[] = "Print In Protect Mode";

  screen_print(msg, sizeof(msg), SCREEN_COLOR_RED);

  return;
}
