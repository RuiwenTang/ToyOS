#include "proc/proc.h"

#include <stddef.h>
#include <string.h>

#include "kpanic.h"
#include "mmu/heap.h"
#include "mmu/page.hpp"
#include "mmu/palloc.h"
#include "util/list.hpp"

#define PROC_MEMORY_BASE 0x80000000
// 1 for PTD and 4 for PT init memory is 16MB
#define PROC_PAGE_MAP_SIZE (0x1000 * 5)
#define PROC_STACK_SIZE (0x1000 * 2)

extern "C" {

// defined in proc.asm
void proc_restart();

}  // extern "C"

struct MemoryRegion {
  uint32_t base = 0;
  uint32_t length = 0;

  MemoryRegion* prev = nullptr;
  MemoryRegion* next = nullptr;
};

struct FileDescriptor {
  fs::Node* node = {};
  FileDescriptor* prev = {};
  FileDescriptor* next = {};
  uint16_t id = 4;  // 1: stdout, 2: stderr, 3: TBD
};

typedef struct proc {
  StackFrame regs;
  int32_t ticks;
  uint32_t pid;

  uint32_t mapd_base;
  uint32_t mapd_length;
  uint32_t page_table;

  uint32_t stack_top;

  util::List<MemoryRegion> memory;
  util::List<FileDescriptor> files;

  char* pwd;

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

void proc_set_pwd(Proc* proc, const char* path) {
  // for now the path must be absolute
  size_t len = strlen(path);

  size_t last_index = len;
  while (path[last_index] != '/' && last_index >= 0) {
    last_index--;
  }

  proc->pwd = (char*)kmalloc(last_index + 2);

  memcpy(proc->pwd, path, last_index + 1);

  proc->pwd[last_index + 1] = '\0';
}

const char* proc_get_pwd(Proc* proc) { return proc->pwd; }

void proc_add_memory(Proc* proc, uint32_t base, uint32_t length) {
  // step 1 add memory region into proc memory region list
  auto region = new MemoryRegion;
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

  // close all files proc opened
  FileDescriptor* fd = proc->files.head;

  while (fd) {
    fd->node->Close();
    proc_remove_file(proc, fd->node);
  }

  // free all allocated page
  MemoryRegion* memory = proc->memory.head;
  while (memory) {
    palloc_free(memory->base, memory->length);

    auto region = memory;

    memory = memory->next;

    delete region;
  }

  kfree(proc);

  // switch to other process
  proc_switch();
}

/**
 * Inserts the given MemoryRegion into the Proc's memory list.
 *
 * @param proc the Proc instance to insert the MemoryRegion into
 * @param region the MemoryRegion instance to be inserted
 *
 */
void proc_insert_memory_region(Proc* proc, MemoryRegion* region) {
  util::List<MemoryRegion>::Insert<&MemoryRegion::prev, &MemoryRegion::next>(
      region, proc->memory.tail, nullptr, &proc->memory.head,
      &proc->memory.tail);
}

/**
 * Removes a memory region from the given Proc.
 *
 * @param proc The Proc from which to remove the memory region.
 * @param base The starting address of the memory region to remove.
 * @param length The length of the memory region to remove.
 *
 */
void proc_remove_memory_region(Proc* proc, uint32_t base, uint32_t length) {
  // find the region
  MemoryRegion* region = proc->memory.head;
  while (region) {
    if (region->base == base && region->length == length) {
      break;
    }
    region = region->next;
  }

  if (region) {
    util::List<MemoryRegion>::Remove<&MemoryRegion::prev, &MemoryRegion::next>(
        region, &proc->memory.head, &proc->memory.tail);
  }
}

uint16_t proc_insert_file(Proc* proc, fs::Node* file) {
  auto head = proc->files.head;

  while (head) {
    if (head->node == file) {
      return head->id;
    }
    head = head->next;
  }

  // insert into list

  auto desc = new FileDescriptor;

  if (proc->files.tail) {
    desc->id = proc->files.tail->id + 1;
  }

  desc->node = file;

  util::List<FileDescriptor>::Insert<&FileDescriptor::prev,
                                     &FileDescriptor::next>(
      desc, proc->files.tail, nullptr, &proc->files.head, &proc->files.tail);
  return desc->id;
}

fs::Node* proc_get_file_by_id(Proc* proc, uint16_t id) {
  auto head = proc->files.head;

  while (head) {
    if (head->id == id) {
      return head->node;
    }
    head = head->next;
  }

  return nullptr;
}

void proc_remove_file(Proc* proc, fs::Node* file) {
  FileDescriptor* desc = nullptr;

  auto head = proc->files.head;

  while (head) {
    if (head->node == file) {
      desc = head;
      break;
    }

    head = head->next;
  }

  util::List<FileDescriptor>::Remove<&FileDescriptor::prev,
                                     &FileDescriptor::next>(
      desc, &proc->files.head, &proc->files.tail);

  if (desc) {
    delete desc->node;
    delete desc;
  }
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

void proc_unmmap_address(proc* proc, uint32_t v_addr, uint32_t size) {
  if (v_addr < PROC_MEMORY_BASE) {
    return;
  }

  size += 0xFFF;
  size &= 0xFFFFF000;

  uint32_t count = size / 0x1000;

  Page* pt = (Page*)proc->page_table;

  uint32_t first_pt_index = (v_addr - PROC_MEMORY_BASE) / 0x1000;
  for (uint32_t i = 0; i < count; i++) {
    pt[first_pt_index + i].present = 0;
    pt[first_pt_index + i].rw = 0;
    pt[first_pt_index + i].user = 0;
    pt[first_pt_index + i].unused = 0;
    pt[first_pt_index + i].address = 0;
  }

  // TODO reduce proc memory length
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

void proc_switch() {
  if (current_proc == NULL) {
    // No process can run, the kernel need to panic
    KERNEL_PANIC("NO available process can run !!");
    return;
  }

  mmu::load_proc(current_proc);
  proc_restart();
}
