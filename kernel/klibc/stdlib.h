#ifndef TOY_KERNEL_LIBC_STDIO_H
#define TOY_KERNEL_LIBC_STDIO_H

#include <stddef.h>
#include <stdint.h>

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

#endif  // TOY_KERNEL_LIBC_STDIO_H