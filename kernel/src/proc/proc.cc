#include "proc/proc.h"

#include <stddef.h>
#include <string.h>

#include "mmu/heap.h"
#include "mmu/page.h"
#include "mmu/palloc.h"
#include "util/list.hpp"

#define PROC_MEMORY_BASE 0x80000000
// 1 for PTD and 4 for PT init memory is 16MB
#define PROC_PAGE_MAP_SIZE (0x1000 * 5)
#define PROC_STACK_SIZE (0x1000 * 2)

typedef struct proc {
  StackFrame regs;
  int32_t ticks;
  uint32_t pid;

  uint32_t mapd_base;
  uint32_t mapd_length;
  uint32_t page_table;

  uint32_t stack_top;

  MemoryRegion* memory;

  proc* ready_prev;
  proc* ready_next;
  proc* suspend_prev;
  proc* suspend_next;
} Proc;

Proc* current_proc = NULL;

util::List<Proc> suspend_list{};
util::List<Proc> ready_list{};

uint32_t proc_count = 0;

typedef Proc*(list_get_next)(Proc*);
typedef void(list_set_next)(Proc*, Proc*);

extern "C" {

static void proc_insert_memory_region(Proc* proc, MemoryRegion* region);

Proc* init_proc(uint32_t init_size) {
  init_size = SIZE_ALIGN_4K(init_size);
  Proc* p = (Proc*)kmalloc(sizeof(Proc));
  memset(p, 0, sizeof(Proc));

  // page mapping for this proc
  uint32_t proc_ptd = palloc_allocate(PROC_PAGE_MAP_SIZE);
  proc_add_memory(p, proc_ptd, PROC_PAGE_MAP_SIZE);

  p->page_table = proc_ptd;
  p->mapd_base = PROC_MEMORY_BASE;
  p->mapd_length = init_size;

  uint32_t p_addr = palloc_allocate(init_size);

  proc_add_memory(p, p_addr, init_size);
  proc_map_address(p, PROC_MEMORY_BASE, p_addr, init_size);
  // init 8k for stack

  uint32_t proc_stack_addr = palloc_allocate(PROC_STACK_SIZE);
  proc_add_memory(p, proc_stack_addr, PROC_STACK_SIZE);
  proc_map_address(p, p->mapd_base + p->mapd_length, proc_stack_addr,
                   PROC_STACK_SIZE);

  p->mapd_length += PROC_STACK_SIZE;

  p->stack_top = p->mapd_base + p->mapd_length - 4;

  return p;
}

void proc_add_memory(Proc* proc, uint32_t base, uint32_t length) {
  // step 1 add memory region into proc memory region list
  MemoryRegion* region = (MemoryRegion*)kmalloc(sizeof(MemoryRegion));
  region->base = base;
  region->length = length;

  proc_insert_memory_region(proc, region);
}

void suspend_proc(Proc* proc) {
  util::List<Proc>::Insert<&Proc::suspend_prev, &Proc::suspend_next>(
      proc, suspend_list.tail, nullptr, &suspend_list.head, &suspend_list.tail);
}

void switch_to_ready(Proc* proc) {
  util::List<Proc>::Insert<&Proc::ready_prev, &Proc::ready_next>(
      proc, nullptr, ready_list.head, &ready_list.head, &ready_list.tail);

  current_proc = ready_list.head;
}

StackFrame* proc_get_stackframe(Proc* proc) { return &proc->regs; }

uint32_t proc_get_stacktop(Proc* proc) { return proc->stack_top; }

uint32_t proc_get_page_table(Proc* proc) { return proc->page_table; }

uint32_t proc_get_maped_base(Proc* proc) { return proc->mapd_base; }

uint32_t proc_get_maped_length(Proc* proc) { return proc->mapd_length; }

void proc_exit(Proc* proc) {
  util::List<Proc>::Remove<&Proc::suspend_prev, &Proc::suspend_next>(
      current_proc, &suspend_list.head, &suspend_list.tail);

  util::List<Proc>::Remove<&Proc::ready_prev, &Proc::ready_next>(
      current_proc, &ready_list.head, &ready_list.tail);

  current_proc = ready_list.head;

  // free all allocated page
  while (proc->memory) {
    MemoryRegion* region = proc->memory;
    proc->memory = proc->memory->next;

    palloc_free(region->base, region->length);

    kfree(region);
  }

  kfree(proc);
}

void proc_insert_memory_region(Proc* proc, MemoryRegion* region) {
  if (proc->memory == NULL) {
    proc->memory = region;
    return;
  }

  MemoryRegion* p = proc->memory;
  while (p->next != NULL) {
    p = p->next;
  }

  p->next = region;
}

Proc* remove_from_list(Proc* list, Proc* item, list_get_next get_next,
                       list_set_next set_next) {
  if (list == item) {
    Proc* tmp = get_next(list);
    set_next(item, NULL);
    return tmp;
  }

  Proc* p = list;

  while (get_next(p) != item) {
    p = get_next(p);
  }

  if (get_next(p) == item) {
    set_next(p, get_next(p));
    set_next(item, NULL);
  }

  return list;
}

Proc* insert_into_list(Proc* list, Proc* item, list_get_next get_next,
                       list_set_next set_next) {
  if (list == NULL) {
    return item;
  }

  set_next(item, list);

  return item;
}

void proc_map_address(Proc* proc, uint32_t v_addr, uint32_t p_addr,
                      uint32_t size) {
  if (v_addr < PROC_MEMORY_BASE) {
    return;
  }

  size += 0xFFF;
  size &= 0xFFFFF000;

  uint32_t count = size / 0x1000;

  Page* pt = (Page*)proc->page_table;

  uint32_t first_pt_index = (v_addr - PROC_MEMORY_BASE) / 0x1000;
  for (uint32_t i = 0; i < count; i++) {
    pt[first_pt_index + i].present = 1;
    pt[first_pt_index + i].rw = 1;
    pt[first_pt_index + i].user = 1;
    pt[first_pt_index + i].unused = 0;
    pt[first_pt_index + i].address = p_addr >> 12;

    p_addr += 0x1000;
  }
}

uint32_t proc_phy_address(Proc* proc, uint32_t v_addr) {
  if (v_addr < PROC_MEMORY_BASE) {
    return 0;
  }
  Page* pt = (Page*)proc->page_table;
  uint32_t pt_index = (v_addr - PROC_MEMORY_BASE) / 0x1000;

  uint32_t p_addr_base = pt[pt_index].address << 12;
  uint32_t p_addr_inner = v_addr & 0xfff;

  return p_addr_base + p_addr_inner;
}

// defined in proc.asm
void proc_restart();

void proc_switch() {
  if (current_proc == NULL) {
    return;
  }

  page_load_proc(current_proc);
  proc_restart();
}

}  // extern "C"