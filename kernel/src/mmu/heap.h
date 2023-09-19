#ifndef KERNEL_MMU_HEAP_H
#define KERNEL_MMU_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void* base, uint32_t size);

void* kmalloc(size_t size);

void* krealloc(void* ptr, size_t size);

void kfree(void*);

#ifdef __cplusplus
}
#endif

#endif  // KERNEL_MMU_HEAP_H
