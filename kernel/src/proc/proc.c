#include "proc/proc.h"

#include <stddef.h>
#include <string.h>

#include "mmu/heap.h"

Proc* current_proc = NULL;
Proc* suspend_list = NULL;

uint32_t proc_count = 0;

Proc* init_proc() {
  Proc* p = (Proc*)kmalloc(sizeof(Proc));
  memset(p, 0, sizeof(Proc));

  return p;
}

void suspend_proc(Proc* proc) {
  proc->suspend_next = suspend_list;
  suspend_list = proc;
}

void switch_to_ready(Proc* proc) {
  // remove fron suspend list
  if (suspend_list == NULL) {
    // no thing todo
    goto end;
  }

  if (suspend_list == proc) {
    suspend_list = suspend_list->suspend_next;
    return;
  }

  Proc* curr = suspend_list;

  while (curr->suspend_next && curr->suspend_next != proc) {
    curr = curr->suspend_next;
  }

  if (curr->suspend_next == proc) {
    curr->suspend_next = proc->suspend_next;
  }

end:
  current_proc = proc;
}
