#include "proc/proc.h"

#include <stddef.h>
#include <string.h>

#include "mmu/heap.h"
#include "mmu/palloc.h"

#define PROC_MEMORY_BASE 0x80000000
// 1 for PTD and 4 for PT init memory is 16MB
#define PROC_PAGE_MAP_SIZE (0x1000 * 5)
#define PROC_STACK_SIZE (0x1000 * 2)

Proc* current_proc = NULL;
Proc* suspend_list = NULL;

uint32_t proc_count = 0;

typedef Proc*(list_get_next)(Proc*);
typedef void(list_set_next)(Proc*, Proc*);

static Proc* suspend_list_get_next(Proc* p) { return p->suspend_next; }

static Proc* ready_list_get_next(Proc* p) { return p->ready_next; }

static void suspend_list_set_next(Proc* p, Proc* next) {
  p->suspend_next = next;
}

static void ready_list_set_next(Proc* p, Proc* next) { p->ready_next = next; }

static void proc_insert_memory_region(Proc* proc, MemoryRegion* region);

static Proc* remove_from_list(Proc* list, Proc* item, list_get_next,
                              list_set_next);

static Proc* insert_into_list(Proc* list, Proc* item, list_get_next,
                              list_set_next);

#define SUSPEND_INSERT(list, node) \
  insert_into_list(list, node, suspend_list_get_next, suspend_list_set_next)

#define SUSPEND_REMOVE(list, node) \
  remove_from_list(list, node, suspend_list_get_next, suspend_list_set_next)

#define READY_INSERT(list, node) \
  insert_into_list(list, node, ready_list_get_next, ready_list_set_next)

#define READY_REMOVE(list, node) \
  remove_from_list(list, node, ready_list_get_next, ready_list_set_next)

static uint32_t proc_calculate_pt(Proc* proc, uint32_t virtual_addr);

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

  page_map_addr(p->page_table, PROC_PAGE_MAP_SIZE, p_addr, init_size);

  // init 8k for stack

  uint32_t proc_stack_addr = palloc_allocate(0x2000);
  proc_add_memory(p, proc_stack_addr, PROC_STACK_SIZE);

  page_map_addr(proc_calculate_pt(p, p->mapd_base + p->mapd_length),
                p->mapd_base + p->mapd_length, proc_stack_addr,
                PROC_STACK_SIZE);

  p->mapd_length += PROC_STACK_SIZE;

  p->stack_top = p->mapd_length;

  return p;
}

void proc_add_memory(Proc* proc, uint32_t base, uint32_t length) {
  // step 1 add memory region into proc memory region list
  MemoryRegion* region = (MemoryRegion*)kmalloc(sizeof(MemoryRegion));
  region->base = base;
  region->length = length;

  proc_insert_memory_region(proc, region);
}

void suspend_proc(Proc* proc) { SUSPEND_INSERT(suspend_list, proc); }

void switch_to_ready(Proc* proc) {
  current_proc = READY_INSERT(current_proc, proc);
}

void proc_exit(Proc* proc) {
  suspend_list = SUSPEND_REMOVE(suspend_list, proc);
  current_proc = READY_REMOVE(current_proc, proc);
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


uint32_t proc_calculate_pt(Proc* proc, uint32_t virtual_addr) {
  virtual_addr &= 0xfff;

  if (virtual_addr < PROC_MEMORY_BASE) {
    // some thing is wrong
    return proc->page_table;
  }

  uint32_t delta = virtual_addr - PROC_MEMORY_BASE;
  delta = delta >> 12;

  return proc->page_table + delta * 4;
}

