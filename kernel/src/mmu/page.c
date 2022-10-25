#include "mmu/page.h"

#include <stddef.h>
#include <stdint.h>

#include "kprintf.h"
#include "mmu/heap.h"
#include "screen/screen.h"

extern const void kernel_start;
extern const void kernel_end;

PageDirectory g_pd[1024] __attribute__((aligned(4096)));

Page* g_page_table_head = NULL;
Page* g_page_table_tail = NULL;

static void page_init_tables(uint32_t total_memory, Framebuffer* info);

static void page_map_screen(Framebuffer* info);

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

  uint32_t kernel_heap = (uint32_t)g_page_table_tail;
  // five page for kernel heap usage
  uint32_t kernel_size = 0x1000 * 5;

  heap_init((void*)kernel_heap, kernel_size);

  kprintf("Print after page enable, screen is mapped!\n");

  uint32_t free_space = kernel_heap + kernel_size;
  kprintf("FreeMemory begin at %x \n", free_space);
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

  g_page_table_tail = current + 1024;
  kprintf("last page_table at: %x \n", (uint32_t)g_page_table_tail);

  page_map_screen(info);
}

void page_map_screen(Framebuffer* info) {
  uint32_t total_memory = info->height * info->pitch;
  total_memory += 0xfff;
  total_memory &= 0xfffff000;

  uint32_t dir_count = (total_memory + 0x3fffff) / 0x400000;
  uint32_t page_count = (total_memory + 0xfff) / 0x1000;

  uint32_t base = info->addr;

  Page* current = g_page_table_tail;

  for (uint32_t i = 0; i < dir_count; i++) {
    if (total_memory == 0 || page_count == 0) {
      break;
    }

    uint32_t dir_index = base / 0x400000;
    g_pd[dir_index].present = 1;
    g_pd[dir_index].rw = 1;
    g_pd[dir_index].user = 0;
    g_pd[dir_index].unused = 0;

    g_pd[dir_index].frame = ((uint32_t)current) >> 12;

    uint32_t j = 0;
    for (; j < 1024; j++) {
      current[j].present = 1;
      current[j].rw = 1;
      current[j].user = 0;
      current[j].unused = 0;

      current[j].address = base >> 12;

      base += 0x1000;
      total_memory -= 0x1000;

      if (total_memory <= 0) {
        break;
      }
    }
    page_count -= j;
    current += j;
  }

  uint32_t current_align = (uint32_t)current;
  current_align += 0xfff;
  current_align &= 0xfffff000;

  g_page_table_tail = (Page*)current_align;

  kprintf("page_tail after map screen : %x \n", (uint32_t)g_page_table_tail);
}
