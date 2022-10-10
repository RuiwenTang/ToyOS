
#include "font8x8.h"

typedef unsigned short uint16;
typedef unsigned char uint8;
typedef unsigned int uint32;

struct vbe_mode_info_structure {
  uint16 attributes; // deprecated, only bit 7 should be of interest to you, and
                     // it indicates the mode supports a linear frame buffer.
  uint8 window_a;    // deprecated
  uint8 window_b;    // deprecated
  uint16 granularity; // deprecated; used while calculating bank numbers
  uint16 window_size;
  uint16 segment_a;
  uint16 segment_b;
  uint32 win_func_ptr; // deprecated; used to switch banks from protected mode
                       // without returning to real mode
  uint16 pitch;        // number of bytes per horizontal line
  uint16 width;        // width in pixels
  uint16 height;       // height in pixels
  uint8 w_char;        // unused...
  uint8 y_char;        // ...
  uint8 planes;
  uint8 bpp;   // bits per pixel in this mode
  uint8 banks; // deprecated; total number of banks in this mode
  uint8 memory_model;
  uint8 bank_size; // deprecated; size of a bank, almost always 64 KB but may be
                   // 16 KB...
  uint8 image_pages;
  uint8 reserved0;

  uint8 red_mask;
  uint8 red_position;
  uint8 green_mask;
  uint8 green_position;
  uint8 blue_mask;
  uint8 blue_position;
  uint8 reserved_mask;
  uint8 reserved_position;
  uint8 direct_color_attributes;

  uint32 framebuffer; // physical address of the linear frame buffer; write here
                      // to draw to the screen
  uint32 off_screen_mem_off;
  uint16 off_screen_mem_size; // size of memory in the framebuffer but not being
                              // displayed on the screen
  uint8 reserved1[206];
} __attribute__((packed));

void clear_screen(uint8 *addr, uint16 width, uint16 height, uint16 pitch) {
  for (uint16 x = 0; x < width; x++) {
    for (uint16 y = 0; y < height; y++) {
      uint8 *line = addr + pitch * y;

      uint8 *pix = line + x * 3;
      pix[0] = 0x0;
      pix[1] = 0x0;
      pix[2] = 0x0;
    }
  }
}

uint8 *g_fb = 0;
uint16 g_width = 0;
uint16 g_height = 0;
uint16 g_pitch = 0;

void put_pixel(uint32 color, uint16 x, uint16 y) {
  uint8 *line = g_fb + g_pitch * y;

  uint8 *pix = line + x * 3;

  pix[0] = color & 0xFF;
  pix[1] = (color >> 8) & 0xFF;
  pix[2] = (color >> 16) & 0xFF;
}

void put_char(char c, uint16 x, uint16 y) {
  uint32 pix = 0x00FFFFFF;

  uint8 *bitmap = (uint8 *)font8x8_basic[c];

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      int set = bitmap[i] & (1 << j);
      if (set) {
        put_pixel(pix, x + j, y + i);
      }
    }
  }
}

void stage2_main(void *info) {
  struct vbe_mode_info_structure *vbe_info =
      (struct vbe_mode_info_structure *)info;

  int *p = (int *)0x9000;

  *p = vbe_info->bpp;

  g_fb = (uint8 *)vbe_info->framebuffer;
  g_width = vbe_info->width;
  g_height = vbe_info->height;
  g_pitch = vbe_info->pitch;

  clear_screen((uint8 *)vbe_info->framebuffer, vbe_info->width,
               vbe_info->height, vbe_info->pitch);

  int x = 0;
  int y = 2;

  char msg[] = "Print In Protect Mode";

  int i = 0;
  for (i; i < sizeof(msg); i++) {
    put_char(msg[i], x, y);
    x += 8;
  }

  return;
}
