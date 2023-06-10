#include "mmu/page.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "kprintf.h"
#include "mmu/heap.h"
#include "mmu/palloc.h"
#include "proc/proc.h"
#include "screen/screen.h"
#include "sys/mman.h"

#define PD_SIZE 0x400000
#define PT_SIZE 0x1000
#define KERNEL_HEEP_SIZE PD_SIZE

// these all defined in asm code
extern "C" {

uint32_t get_kernel_start();
uint32_t get_kernel_end();

void page_load_directory(void* pdr);
void page_enable();

}  // extern "C"

PageDirectory g_pd[1024] __attribute__((aligned(4096)));

Page* g_page_table_head = NULL;
Page* g_page_table_tail = NULL;

uint32_t g_total_memory = 0;

namespace mmu {

template <typename T>
uint32_t Align4k(T value) {
  return (reinterpret_cast<uint32_t>(value) + 0xfff) & ~0xfff;
}

/**
 * Initializes page tables with a 1:1 map in kernel.
 *
 * @param total_memory the total amount of memory to map
 */
void init_page_tables(uint32_t total_memory) {
  // 1 : 1 map in kernel
  Page* current = NULL;
  uint32_t i = 0;
  for (i = 0; i < 1024; i++) {
    if (total_memory > 0) {
      g_pd[i].present = 1;
      g_pd[i].rw = 1;
      g_pd[i].user = 1;
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
        current[j].user = 1;
        current[j].unused = 0;
        current[j].address = (addr >> 12);

        total_memory -= 0x1000;
      } else {
        current[j].present = 0;
      }
    }
  }

  g_page_table_tail = current + 1024;
}

/**
 * Map screen memory 1:1 in kernel
 *
 * @param info      the multiboot info
 * @return uint32_t memory end address about video memory
 */
uint32_t map_screen(multiboot_info_t* info) {
  uint32_t total_memory = info->framebuffer_height * info->framebuffer_pitch;
  total_memory += 0xfff;
  total_memory &= 0xfffff000;

  uint32_t dir_count = (total_memory + 0x3fffff) / 0x400000;
  uint32_t page_count = (total_memory + 0xfff) / 0x1000;

  uint32_t base = info->framebuffer_addr;

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

  return base + total_memory;
}

void Init(multiboot_info_t* info) {
  uint32_t kernel_end = get_kernel_end();
  kprintf("kernel end = %x \n", kernel_end);
  // page dir address
  g_page_table_head = reinterpret_cast<Page*>(Align4k(kernel_end));

  // calculate total memory size based on multiboot info
  uint32_t total_memory = 0;
  uint32_t mmap_count = info->mmap_length / sizeof(struct multiboot_mmap_entry);

  auto mmap_ptr =
      reinterpret_cast<struct multiboot_mmap_entry*>(info->mmap_addr);
  for (uint32_t i = 0; i < mmap_count; i++) {
    kprintf("MemoryRegion: base: %x | length %x | type: %d\n",
            (uint32_t)mmap_ptr->addr, (uint32_t)mmap_ptr->len, mmap_ptr->type);

    total_memory += mmap_ptr->len;
    mmap_ptr++;
  }

  // align total memory by 4MB aka PD_SIZE
  total_memory += 0x003FFFFF;
  total_memory &= (~0x003FFFFF);

  // initialize page directory
  init_page_tables(total_memory);
  // map screen memmory
  map_screen(info);

  // init kernel
  uint32_t kernel_heap_start = (uint32_t)g_page_table_tail;
  uint32_t kernel_heap_end = kernel_heap_start + KERNEL_HEEP_SIZE;

  heap_init(reinterpret_cast<void*>(kernel_heap_start), KERNEL_HEEP_SIZE);

  palloc_init(kernel_heap_end, total_memory - kernel_heap_end);

  // load page directory
  page_load_directory(g_pd);

  page_enable();
}

void load_proc(Proc* p) {
  uint32_t pd_count = (proc_get_maped_length(p) / PD_SIZE) + 1;

  uint32_t pd_index = proc_get_maped_base(p) / PD_SIZE;

  uint32_t proc_pt = proc_get_page_table(p);
  for (uint32_t i = 0; i < pd_count; i++) {
    g_pd[pd_index + i].user = 1;
    g_pd[pd_index + i].rw = 1;
    g_pd[pd_index + i].present = 1;
    g_pd[pd_index + i].unused = 0;
    g_pd[pd_index + i].frame = proc_pt >> 12;

    proc_pt += 0x1000;
  }
}

/**
 *  ebx = size of mapped memory
 *  ecx = phy addr to map can be zero
 *  edx = flags
 */
void sys_call_mmap(StackFrame* frame) {
  uint32_t ebx = frame->ebx;  // size
  uint32_t ecx = frame->ecx;  // phy addr
  uint32_t edx = frame->edx;  // flags

  auto proc = reinterpret_cast<Proc*>(frame);

  // currently only support anon map with no phy addr

  if (ecx != 0 || (edx & MAP_ANON) == 0) {
    // not support yet
    return;
  }

  auto size = Align4k(ebx);

  if (size == 0) {
    // request size is zero
    return;
  }

  uint32_t p_addr = palloc_allocate(size);

  if (p_addr == 0) {
    // out of memory
    return;
  }

  // first add this memory range into pcb
  proc_add_memory(proc, p_addr, size);

  // map this memory to proc address space at end of proc's memory
  uint32_t v_addr = proc_get_maped_base(proc) + proc_get_maped_length(proc);
  v_addr = Align4k(v_addr);
  proc_map_address(proc, v_addr, p_addr, size);

  // return v_addr to caller
  frame->eax = v_addr;
}

}  // namespace mmu
