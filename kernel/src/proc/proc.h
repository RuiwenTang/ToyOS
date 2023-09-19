#ifndef TOY_PROC_PROC_H
#define TOY_PROC_PROC_H

#include "fs/vfs.hpp"
#include "proc/stack_frame.h"

struct proc;

typedef struct proc Proc;

/**
 * @brief alloc a process control block
 *
 * @return Proc* new proc struct
 */
Proc* init_proc(uint32_t init_size);

void proc_set_pwd(Proc* proc, const char* path);

const char* proc_get_pwd(Proc* proc);

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

uint16_t proc_insert_file(Proc* proc, fs::Node* file);

fs::Node* proc_get_file_by_id(Proc* proc, uint16_t id);

void proc_remove_file(Proc* proc, fs::Node* file);

void proc_map_address(Proc* proc, uint32_t v_addr, uint32_t p_addr,
                      uint32_t size);

void proc_unmmap_address(proc* proc, uint32_t v_addr, uint32_t size);

uint32_t proc_phy_address(Proc* proc, uint32_t v_addr);

void proc_switch();

void proc_exit(Proc* proc);

#endif  // TOY_PROC_PROC_H
