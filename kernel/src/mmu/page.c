#include "mmu/page.h"

#include <stddef.h>
#include <stdint.h>

#include "kprintf.h"
#include "screen/screen.h"

extern const void kernel_start;
extern const void kernel_end;

// may be this should set in screen.c
uint32_t screen_phy_base = 0x400000;

PageDirectory g_pd[1024] __attribute__((aligned(4096)));

Page* g_page_table_head = NULL;

static void page_init_tables(uint32_t total_memory, Framebuffer* info);

static void page_map_screen(Framebuffer* info, uint32_t base, Page* page_table);

void page_init(BootInfo* info) {
  g_page_table_head = (Page*)((((uint32_t)&kernel_end) + 0xfff) & 0xFFFFF000);
  uint32_t total_memory = 0;
  for (uint32_t i = 0; i < info->memory_info_count; i++) {
    kprintf("MemoryRegion: base: %x | length %x | type: %d\n",
            (uint32_t)info->memory_info[i].base,
            (uint32_t)info->memory_info[i].length, info->memory_info[i].type);
    total_memory += info->memory_info[i].length;
  }

  // align total memory by 4MB
  total_memory += 0x003FFFFF;
  total_memory &= (~0x003FFFFF);

  kprintf("Total memory is %d MB \n", total_memory / (1024 * 1024));

  kprintf("Kernel start at : %x | end at %x \n", (uint32_t)&kernel_start,
          (uint32_t)&kernel_end);

  kprintf("g_pd at %x | first page_table at %x \n", (uint32_t)g_pd,
          (uint32_t)g_page_table_head);

  page_init_tables(total_memory, &info->frame_buffer);

  page_load_directory(g_pd);

  page_enable();

  kprintf("aaaaa \n");
}

void page_init_tables(uint32_t total_memory, Framebuffer* info) {
  // 1 : 1 map in kernel
  Page* current = NULL;
  uint32_t i = 0;
  for (i = 0; i < 1024; i++) {
    if (total_memory > 0) {
      g_pd[i].present = 1;
      g_pd[i].rw = 1;
      g_pd[i].user = 0;
      g_pd[i].ps = 0;
      g_pd[i].unused = 0;
    } else {
      g_pd[i].present = 0;
      continue;
    }
    g_pd[i].frame = ((uint32_t)(g_page_table_head + i * 1024)) >> 12;
    current = g_page_table_head + i * 1024;

    for (uint32_t j = 0; j < 1024; j++) {
      if (total_memory > 0) {
        uint32_t addr = i * 1024 * 1024 * 4 + j * 4096;

        current[j].present = 1;
        current[j].rw = 1;
        current[j].user = 0;
        current[j].unused = 0;
        current[j].address = (addr >> 12);

        total_memory -= 0x1000;
      } else {
        current[j].present = 0;
      }
    }
  }

  kprintf("last page_table at: %x \n", (uint32_t)current);

  screen_phy_base = (uint32_t)current;
  screen_phy_base += 0xFFFFF;
  screen_phy_base &= 0xFFF00000;

  kprintf("screen_phy_base = %x \n", screen_phy_base);

  uint32_t memory = info->height * info->pitch;

  uint32_t page_count = (memory + 0xfff) / 0x1000;

  uint32_t base = info->addr;

  uint32_t pd_i = screen_phy_base / (4 * 1024 * 1024);

  uint32_t pt_i = (screen_phy_base >> 12) % 0x1000;

  kprintf("pd_i = %d | pt_i = %d \n", pd_i, pt_i);

  Page* page = (Page*)(g_pd[pd_i].frame << 12);
  page += pt_i;

  kprintf("start page at : %x \n", (uint32_t)page);

  for (uint32_t i = 0; i < page_count; i++) {
    page[i].address = (base >> 12);
    page[i].present = 1;
    page[i].rw = 1;

    base += 0x1000;
  }

  screen_update_base(screen_phy_base);
}
