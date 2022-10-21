#ifndef KERNEL_MMU_HEAP_H
#define KERNEL_MMU_HEAP_H

#include <stddef.h>

#include "mmu/page.h"

void heap_init(void* base, uint32_t size);

void* kmalloc(size_t size);

void kfree(void*);

#endif  // KERNEL_MMU_HEAP_H