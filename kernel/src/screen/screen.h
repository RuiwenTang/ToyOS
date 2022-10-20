#ifndef KERNEL_SCREEN_SCREEN_H
#define KERNEL_SCREEN_SCREEN_H

#include <boot/toy_boot.h>

#define SCREEN_COLOR_WHITE 0xffffff

#define SCREEN_COLOR_RED 0xff0000

void screen_init(Framebuffer* fb_info);

void screen_set_color(uint32_t color);

void screen_clear();

void screen_print(char* str, uint32_t len, uint32_t color);

void screen_update_base(uint32_t base);

#endif  // KERNEL_SCREEN_SCREEN_H