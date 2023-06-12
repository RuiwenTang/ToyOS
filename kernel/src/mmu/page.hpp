#ifndef TOY_MMU_PAGE_H
#define TOY_MMU_PAGE_H

#include <boot/multiboot.h>
#include <stdint.h>

#include "proc/stack_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// proc struct
struct proc;

// page table entry
typedef struct {
  uint32_t present : 1;  // Page present in memory
  uint32_t rw : 1;       // Read-Only if clear, readwrite if set
  uint32_t user : 1;     // Supervisor level only if clear
  uint32_t pwt : 1;      // Page write through
  uint32_t pcd : 1;      // cache disable bit.
  uint32_t access : 1;   // access flag
  uint32_t dirty : 1;    // dirty flag
  uint32_t ps : 1;       // page size bit, if set this maps 4 MB memory not page
                         // table address
  uint32_t global : 1;   // global bits
  uint32_t unused : 3;   // unused bits
  uint32_t frame : 20;   // Frame address (shifted right 12 bits)
} PageDirectory;

typedef struct {
  uint32_t present : 1;   // Page present in memory
  uint32_t rw : 1;        // Read-Only if clear, readwrite if set
  uint32_t user : 1;      // Supervisor level only if clear
  uint32_t pwd : 1;       // Page write through
  uint32_t pcd : 1;       // cache disable bit
  uint32_t access : 1;    // access flag
  uint32_t dirty : 1;     // dirty flags
  uint32_t pat : 1;       // page attribute flags should be 0
  uint32_t global : 1;    // global bits
  uint32_t unused : 3;    // unused
  uint32_t address : 20;  // physical address
} Page;

#ifdef __cplusplus
}
#endif

namespace mmu {

/**
 * Init page mapping
 *
 * @param info  multiboot info
 */
void Init(multiboot_info_t* info);

void load_proc(struct proc*);

void sys_call_mmap(StackFrame* frame);

void sys_call_unmmap(StackFrame* frame);

}  // namespace mmu

#endif  // TOY_MMU_PAGE_H
