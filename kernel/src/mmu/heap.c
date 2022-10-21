#include <stddef.h>
#include <umm_malloc.h>

#include "mmu/page.h"

void* UMM_MALLOC_CFG_HEAP_ADDR = NULL;

uint32_t UMM_MALLOC_CFG_HEAP_SIZE = 0;

void heap_init(void* base, uint32_t size) {
  UMM_MALLOC_CFG_HEAP_ADDR = base;
  UMM_MALLOC_CFG_HEAP_SIZE = size;

  umm_init();
}

void* kmalloc(size_t size) { return umm_malloc(size); }

void kfree(void* ptr) { umm_free(ptr); }
