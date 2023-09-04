#ifndef KERNEL_PRINTF_H
#define KERNEL_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

// simple implement just used in kernel for debug
// https://github.com/limine-bootloader/limine/blob/trunk/common/lib/print.s2.c
void kprintf(const char* fmt, ...);

void kpanicf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif  // KERNEL_PRINTF_H
