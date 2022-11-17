#ifndef TOY_KERNEL_SYSCALL_SYSCALL_H
#define TOY_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

#include "proc/proc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_CALL_INT 0x80

void sys_call_init();

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_SYSCALL_SYSCALL_H
