#ifndef BOOT_SCREEN_H
#define BOOT_SCREEN_H

#include "bios_vbe.h"

#define SCREEN_COLOR_WHITE 0xffffff
#define SCREEN_COLOR_RED 0xff0000

void screen_init(struct vbe_mode_info_structure *info);

void screen_clear();

void screen_print(char *str, uint32_t len, uint32_t color);

#endif // BOOT_SCREEN_H