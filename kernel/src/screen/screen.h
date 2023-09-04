#ifndef KERNEL_SCREEN_SCREEN_H
#define KERNEL_SCREEN_SCREEN_H

#include <boot/multiboot.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCREEN_COLOR_WHITE 0xffffff

#define SCREEN_COLOR_RED 0xff0000
#define SCREEN_COLOR_GREEN 0x00ff00

void screen_init(multiboot_info_t* mb_info);

void screen_set_color(uint32_t color);

void screen_clear();

void screen_print(char* str, uint32_t len, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif  // KERNEL_SCREEN_SCREEN_H
