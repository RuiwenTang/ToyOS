#ifndef TOY_KERNEL_SYSCALL_SYSCALL_H
#define TOY_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_CALL_INT 0x80

#define SYS_CALL_MMAP 2

void sys_call_init();

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_SYSCALL_SYSCALL_H
