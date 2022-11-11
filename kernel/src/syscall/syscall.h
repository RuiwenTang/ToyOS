#ifndef TOY_KERNEL_SYSCALL_SYSCALL_H
#define TOY_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

#include "proc/proc.h"

#define SYS_CALL_INT 0x80

void sys_call_init();

#endif  // TOY_KERNEL_SYSCALL_SYSCALL_H