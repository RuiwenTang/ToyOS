
#include "screen/screen.h"
#include "font8x8_basic.h"

struct ScreenInfo {
  uint8_t bpp;
  uint16_t width;
  uint16_t height;
  uint16_t pitch;
  uint8_t *framebuffer; // addr of framebuffer address

  // logical position for text render
  uint16_t x;
  uint16_t y;
};

struct ScreenInfo g_screen_info;

void screen_init(struct vbe_mode_info_structure *info) {
  g_screen_info.bpp = info->bpp / 8;
  g_screen_info.width = info->width;
  g_screen_info.height = info->height;
  g_screen_info.pitch = info->pitch;
  g_screen_info.framebuffer = (uint8_t *)info->framebuffer;

  g_screen_info.x = 0;
  g_screen_info.y = 2;
}

void screen_clear() {
  for (uint16_t x = 0; x < g_screen_info.width; x++) {
    for (uint16_t y = 0; y < g_screen_info.height; y++) {
      uint8_t *line = g_screen_info.framebuffer + g_screen_info.pitch * y;

      uint8_t *pix = line + x * g_screen_info.bpp;

      for (int32_t i = 0; i < g_screen_info.bpp; i++) {
        pix[i] = 0;
      }
    }
  }

  g_screen_info.x = 0;
  g_screen_info.y = 0;
}

void screen_put_pixel(uint32_t color, uint16_t x, uint16_t y) {
  uint8_t *line = g_screen_info.framebuffer + g_screen_info.pitch * y;

  uint8_t *pix = line + x * g_screen_info.bpp;

  if (g_screen_info.bpp != 3) {
    // FIXME no support for other color space
    for (int32_t i = 0; i < g_screen_info.bpp; i++) {
      pix[i] = 0xFF;
    }
  } else {
    pix[0] = color & 0xFF;
    pix[1] = (color >> 8) & 0xFF;
    pix[2] = (color >> 16) & 0xFF;
  }
}

void screen_put_char(char c, uint32_t color, uint16_t x, uint16_t y) {

  uint8_t *bitmap = (uint8_t *)font8x8_basic[c];

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      int set = bitmap[i] & (1 << j);
      if (set) {
        screen_put_pixel(color, x + j, y + i);
      }
    }
  }
}

void screen_print(char *str, uint32_t len, uint32_t color) {
  uint16_t curr_x = g_screen_info.x;
  uint16_t curr_y = g_screen_info.y;

  for (uint32_t i = 0; i < len; i++) {

    if (str[i] == '\n') {
      curr_x = 0;
      curr_y += 10;
      continue;
    }

    screen_put_char(str[i], color, curr_x, curr_y);
    curr_x += 8;
    if (curr_x > g_screen_info.width) {
      curr_x = 0;
      curr_y += 10;
    }
  }

  g_screen_info.x = curr_x;
  g_screen_info.y = curr_y;
}
