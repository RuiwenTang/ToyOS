
#include <stddef.h>
#include <umm_malloc.h>

extern "C" {

void* ext4_user_malloc(size_t size) { return umm_malloc(size); }

void* ext4_user_calloc(size_t num, size_t size) {
  return umm_calloc(num, size);
}

void* ext4_user_realloc(void* ptr, size_t size) {
  return umm_realloc(ptr, size);
}

void ext4_user_free(void* ptr) { umm_free(ptr); }
}