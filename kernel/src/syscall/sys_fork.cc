#include "syscall/sys_fork.hpp"

#include "proc/proc.h"

namespace sys {

void sys_call_fork(StackFrame* frame) {
  auto proc = reinterpret_cast<Proc*>(frame);

  auto sub_proc = proc_fork(proc);

  switch_to_ready(sub_proc);

  frame->eax = proc_get_pid(sub_proc);

  proc_get_stackframe(sub_proc)->eax = 0;

  proc_add_child(proc, sub_proc);

  proc_switch();
}

}  // namespace sys