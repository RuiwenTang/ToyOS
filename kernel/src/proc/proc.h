#ifndef TOY_PROC_PROC_H
#define TOY_PROC_PROC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stackframe {
  // pushed by isr handler
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;
  // pushed by pusha
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t kernel_esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  // pushed by isr handler stub
  uint32_t interrupt;
  uint32_t error;
  // pushed by cpu
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;
} StackFrame;

struct proc;

typedef struct proc Proc;

/**
 * @brief alloc a process control block
 *
 * @return Proc* new proc struct
 */
Proc* init_proc(uint32_t init_size);

void proc_add_memory(Proc* proc, uint32_t base, uint32_t length);

/**
 * @brief insert proc into suspend list
 *
 * @param proc
 */
void suspend_proc(Proc* proc);

void switch_to_ready(Proc* proc);

StackFrame* proc_get_stackframe(Proc* proc);

uint32_t proc_get_stacktop(Proc* proc);

uint32_t proc_get_page_table(Proc* proc);

uint32_t proc_get_maped_base(Proc* proc);

uint32_t proc_get_maped_length(Proc* proc);

void proc_map_address(Proc* proc, uint32_t v_addr, uint32_t p_addr,
                      uint32_t size);

uint32_t proc_phy_address(Proc* proc, uint32_t v_addr);

void proc_switch();

void proc_exit(Proc* proc);

#ifdef __cplusplus
}
#endif

#endif  // TOY_PROC_PROC_H
