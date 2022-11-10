#ifndef TOY_MMU_PALLOC_H
#define TOY_MMU_PALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIZE_ALIGN_4K(s) ((s + 0xfff) & 0xFFFFF000)

typedef struct Region {
  uint32_t base;
  uint32_t length;
  struct Region* next;
} Region;

void palloc_init(uint32_t base, uint32_t length);

uint32_t palloc_allocate(uint32_t size);

void palloc_free(uint32_t base, uint32_t size);

#ifdef TOY_DEBUG
Region* free_list();

Region* used_list();
#endif

#ifdef __cplusplus
}
#endif

#endif  // TOY_MMU_PALLOC_H
