#include "screen/screen.h"

#include "font8x8_basic.h"

struct KernelScreen {
  uint32_t addr;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t bpp;

  uint32_t pos_x;
  uint32_t pos_y;
  uint32_t color;
};

struct KernelScreen g_kernel_screen;

void screen_init(multiboot_info_t *mb_info) {
  g_kernel_screen.addr = mb_info->framebuffer_addr;
  g_kernel_screen.width = mb_info->framebuffer_width;
  g_kernel_screen.height = mb_info->framebuffer_height;
  g_kernel_screen.pitch = mb_info->framebuffer_pitch;
  g_kernel_screen.bpp = mb_info->framebuffer_bpp / 8;

  g_kernel_screen.pos_x = 0;
  g_kernel_screen.pos_y = 0;

  g_kernel_screen.color = 0;

  screen_set_color(SCREEN_COLOR_WHITE);
  screen_clear();
}

void screen_set_color(uint32_t color) { g_kernel_screen.color = color; }

void screen_clear() {
  uint8_t *addr = (uint8_t *)g_kernel_screen.addr;

  for (uint32_t x = 0; x < g_kernel_screen.width; x++) {
    for (uint32_t y = 0; y < g_kernel_screen.height; y++) {
      uint8_t *pix = addr + y * g_kernel_screen.pitch + x * g_kernel_screen.bpp;

      for (uint32_t i = 0; i < g_kernel_screen.bpp; i++) {
        pix[i] = 0x0;
      }
    }
  }
}

static void screen_put_pixel(uint32_t color, uint16_t x, uint16_t y) {
  uint8_t *line = (uint8_t *)(g_kernel_screen.addr + g_kernel_screen.pitch * y);

  uint8_t *pix = line + x * g_kernel_screen.bpp;

  if (g_kernel_screen.bpp == 4) {
    pix[0] = color & 0xFF;
    pix[1] = (color >> 8) & 0xFF;
    pix[2] = (color >> 16) & 0xFF;
    pix[3] = 0xFF;
  } else if (g_kernel_screen.bpp == 3) {
    pix[0] = color & 0xFF;
    pix[1] = (color >> 8) & 0xFF;
    pix[2] = (color >> 16) & 0xFF;
  } else {
    // FIXME no support for other color space
    for (int32_t i = 0; i < g_kernel_screen.bpp; i++) {
      pix[i] = 0xFF;
    }
  }
}

static void screen_put_char(char c, uint32_t color, uint16_t x, uint16_t y) {
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
  uint32_t curr_x = g_kernel_screen.pos_x;
  uint32_t curr_y = g_kernel_screen.pos_y;

  for (uint32_t i = 0; i < len; i++) {
    if (str[i] == '\n') {
      curr_x = 0;
      curr_y += 10;
      continue;
    }

    if (str[i] == 0) {
      continue;
    }

    screen_put_char(str[i], color, curr_x, curr_y);
    curr_x += 8;
    if (curr_x > g_kernel_screen.width) {
      curr_x = 0;
      curr_y += 10;
    }

    if (curr_y > g_kernel_screen.height) {
      curr_y = 0;
    }
  }

  g_kernel_screen.pos_x = curr_x;
  g_kernel_screen.pos_y = curr_y;
}
