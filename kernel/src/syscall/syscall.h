#ifndef TOY_KERNEL_SYSCALL_SYSCALL_H
#define TOY_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_CALL_INT 0x80

#define SYS_CALL_EXIT 0
#define SYS_CALL_WRITE 1
#define SYS_CALL_MMAP 2
#define SYS_CALL_UNMAP 3
#define SYS_CALL_OPEN 4
#define SYS_CALL_CLOSE 5
#define SYS_CALL_READ 6

void sys_call_init();

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_SYSCALL_SYSCALL_H
