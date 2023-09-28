#ifndef TOY_KERNEL_INCLUDE_SYS_PANIC_H
#define TOY_KERNEL_INCLUDE_SYS_PANIC_H

#include "kprintf.h"

#ifdef __cplusplus
extern "C" {
#endif

// defined in entry.asm
void kernel_hang();

#ifdef __cplusplus
}
#endif

#define KERNEL_PANIC(...)                                           \
  do {                                                              \
    kpanicf("kernel panic at %s, line: %d \n", __FILE__, __LINE__); \
    kpanicf(__VA_ARGS__);                                           \
    kernel_hang();                                                  \
  } while (0)

#endif  // TOY_KERNEL_INCLUDE_SYS_PANIC_H
