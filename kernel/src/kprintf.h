#ifndef KERNEL_PRINTF_H
#define KERNEL_PRINTF_H

// simple implement just used in kernel for debug
// https://github.com/limine-bootloader/limine/blob/trunk/common/lib/print.s2.c
void kprintf(const char *, ...);

#endif  // KERNEL_PRINTF_H