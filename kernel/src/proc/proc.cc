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

#define PROC_MAX_TICKET 10

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

void file_desc_retain(struct FileDescriptor* fd) { fd->node->ReleaseOpen(); }

fs::Node* file_desc_get_file(struct FileDescriptor* fd) { return fd->node; }

uint16_t file_desc_get_id(struct FileDescriptor* fd) { return fd->id; }

enum class ProcState {
  NONE,
  READY,
  SUSPEND,
  EXIT,
};

typedef struct proc {
  StackFrame regs;
  int32_t ticks;
  uint32_t pid;

  ProcState state;

  uint32_t mapd_base;
  uint32_t mapd_length;
  uint32_t page_table;

  uint32_t stack_top;

  bool wait_returned = false;
  uint32_t wait_pid = 0;
  uint32_t wait_state = 0;

  util::List<MemoryRegion> memory;
  util::List<FileDescriptor> files;
  util::List<Proc> children;

  char* pwd;

  proc* parent;

  proc* ready_prev;
  proc* ready_next;
  proc* suspend_prev;
  proc* suspend_next;
  proc* parent_prev;
  proc* parent_next;
} Proc;

Proc* current_proc = NULL;

util::List<Proc> suspend_list{};
util::List<Proc> ready_list{};

uint32_t g_proc_id = 0;

typedef Proc*(list_get_next)(Proc*);
typedef void(list_set_next)(Proc*, Proc*);

static void proc_insert_memory_region(Proc* proc, MemoryRegion* region);

Proc* init_proc(uint32_t init_size) {
  init_size = SIZE_ALIGN_4K(init_size);
  Proc* p = (Proc*)kmalloc(sizeof(Proc));
  memset(p, 0, sizeof(Proc));
  g_proc_id++;
  p->pid = g_proc_id;
  p->ticks = PROC_MAX_TICKET;

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
  util::List<Proc>::Remove<&Proc::ready_prev, &Proc::ready_next>(
      proc, &ready_list.head, &ready_list.tail);

  util::List<Proc>::Insert<&Proc::suspend_prev, &Proc::suspend_next>(
      proc, suspend_list.tail, nullptr, &suspend_list.head, &suspend_list.tail);

  proc->state = ProcState::SUSPEND;
}

void switch_to_ready(Proc* proc) {
  util::List<Proc>::Insert<&Proc::ready_prev, &Proc::ready_next>(
      proc, nullptr, ready_list.head, &ready_list.head, &ready_list.tail);

  util::List<Proc>::Remove<&Proc::suspend_prev, &Proc::suspend_next>(
      proc, &suspend_list.head, &suspend_list.tail);

  current_proc = ready_list.head;

  proc->state = ProcState::READY;
}

Proc* proc_get_parent(Proc* proc) { return proc->parent; }

void proc_add_child(Proc* proc, Proc* child) {
  child->parent = proc;

  util::List<Proc>::Insert<&Proc::parent_prev, &Proc::parent_next>(
      child, nullptr, proc->children.tail, &proc->children.head,
      &proc->children.tail);
}

void proc_remove_child(Proc* proc, Proc* child) {
  if (child->parent == nullptr || child->parent != proc) {
    return;
  }

  util::List<Proc>::Remove<&Proc::parent_prev, &Proc::parent_next>(
      child, &proc->children.head, &proc->children.tail);

  child->parent = nullptr;
}

uint32_t proc_get_pid(Proc* proc) { return proc->pid; }

StackFrame* proc_get_stackframe(Proc* proc) { return &proc->regs; }

uint32_t proc_get_stacktop(Proc* proc) { return proc->stack_top; }

uint32_t proc_get_page_table(Proc* proc) { return proc->page_table; }

uint32_t proc_get_maped_base(Proc* proc) { return proc->mapd_base; }

uint32_t proc_get_maped_length(Proc* proc) { return proc->mapd_length; }

void proc_grow_maped_length(Proc* proc, uint32_t size) {
  proc->mapd_length += size;
}

void proc_exit(Proc* proc) {
  util::List<Proc>::Remove<&Proc::suspend_prev, &Proc::suspend_next>(
      proc, &suspend_list.head, &suspend_list.tail);

  util::List<Proc>::Remove<&Proc::ready_prev, &Proc::ready_next>(
      proc, &ready_list.head, &ready_list.tail);

  current_proc = ready_list.head;

  while (proc->children.head) {
    auto child = proc->children.head;
    proc_exit(child);

    proc_remove_child(proc, child);
    kfree(child);
  }

  // close all files proc opened
  FileDescriptor* fd = proc->files.head;

  while (fd) {
    fd->node->ReleaseOpen();

    if (fd->node->GetOpenCount() <= 0) {
      fd->node->Close();

      delete fd->node;
    }

    auto ptr = fd;

    fd = fd->next;

    delete ptr;
  }

  // free all allocated page
  MemoryRegion* memory = proc->memory.head;
  while (memory) {
    palloc_free(memory->base, memory->length);

    auto region = memory;

    memory = memory->next;

    delete region;
  }
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

struct FileDescriptor* proc_get_fd_by_path(Proc* proc, const char* path) {
  auto head = proc->files.head;

  while (head) {
    if (strcmp(head->node->GetName(), path) == 0) {
      return head;
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

void proc_copy_page_and_map(Proc* new_proc, Proc* proc, uint32_t p_addr) {
  if (new_proc->mapd_length != proc->mapd_length) {
    kpanicf("two process has different memory size !!");
  }

  uint32_t count = proc->mapd_length / 0x1000;

  auto new_pt = reinterpret_cast<Page*>(new_proc->page_table);
  auto old_pt = reinterpret_cast<Page*>(proc->page_table);

  for (uint32_t i = 0; i < count; i++) {
    new_pt[i] = old_pt[i];
    new_pt[i].address = p_addr >> 12;

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

Proc* proc_fork(Proc* proc) {
  Proc* new_proc = reinterpret_cast<Proc*>(kmalloc(sizeof(Proc)));

  memset(new_proc, 0, sizeof(Proc));

  g_proc_id++;

  new_proc->pid = g_proc_id;
  new_proc->mapd_base = proc->mapd_base;
  new_proc->mapd_length = proc->mapd_length;
  new_proc->stack_top = proc->stack_top;
  new_proc->ticks = proc->ticks;

  new_proc->page_table = palloc_allocate(PROC_PAGE_MAP_SIZE);

  // allocate physical memory for new process and copy parent memory into it
  uint32_t p_addr = palloc_allocate(new_proc->mapd_length);

  memcpy(reinterpret_cast<void*>(p_addr),
         reinterpret_cast<void*>(proc->mapd_base), new_proc->mapd_length);

  proc_add_memory(new_proc, p_addr, new_proc->mapd_length);

  proc_copy_page_and_map(new_proc, proc, p_addr);

  // copy all registers
  memcpy(reinterpret_cast<void*>(&new_proc->regs),
         reinterpret_cast<void*>(&proc->regs), sizeof(StackFrame));

  // copy all opened files
  auto fd = proc->files.head;

  while (fd) {
    auto new_fd = new FileDescriptor;
    new_fd->node = fd->node;
    new_fd->node->RetainOpen();

    new_fd->id = fd->id;

    util::List<FileDescriptor>::Insert<&FileDescriptor::prev,
                                       &FileDescriptor::next>(
        new_fd, new_proc->files.tail, nullptr, &new_proc->files.head,
        &new_proc->files.tail);

    fd = fd->next;
  }

  return new_proc;
}

extern "C" void proc_schedule() {
  // timer is filed but no proc is executed
  if (!current_proc) {
    return;
  }

  current_proc->ticks--;
  if (current_proc->ticks <= 0) {
    current_proc->ticks = PROC_MAX_TICKET;

    util::List<Proc>::Remove<&Proc::ready_prev, &Proc::ready_next>(
        current_proc, &ready_list.head, &ready_list.tail);

    switch_to_ready(current_proc);

    proc_switch();
  }
}

void proc_notify_parent(Proc* proc) {
  if (proc->parent == nullptr) {
    return;
  }

  proc->parent->wait_returned = true;
  proc->parent->wait_pid = proc->pid;
  proc->parent->wait_state = proc->regs.ebx;

  if (proc->parent->state == ProcState::SUSPEND) {
    switch_to_ready(proc->parent);
  }
}

void proc_wait(Proc* proc) {
  // child proc returned before parent call wait
  if (proc->wait_returned) {
    proc->regs.eax = proc->wait_pid;

    int* ptr = reinterpret_cast<int*>(proc->regs.ebx);

    if (ptr) {
      *ptr = proc->wait_state;
    }

    proc->wait_returned = false;

    return;
  }

  if (proc->children.head == nullptr) {
    // no actived children and not returned value stored
    proc->regs.eax = -1;
    return;
  }

  // children is running suspend self and wait for children
  suspend_proc(proc);
  current_proc = ready_list.head;

  proc_switch();
}
