/*
 * heap_provider_static.c — R1 kernel heap provider: static arena bump
 *
 * heap.c itself is the unmodified ToyOS64 copy and only talks to pages
 * through heap_provider_t. Until the mm port (R2) supplies a pmm-backed
 * provider, pages come from a fixed .bss arena: a bump allocator whose
 * contiguity is exactly what heap_grow's extend-in-place path wants.
 *
 * free_pages is a no-op: the arena never shrinks, and heap.c reuses freed
 * *blocks* internally, so R1's small kernel-thread allocations recycle
 * fine. kmain swaps nothing at runtime — R2 replaces only this file.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/heap.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* 1 MiB is orders of magnitude beyond R1 needs (TCBs are ~200 B; a handful
 * of kernel threads exist before R3's userland). */
#define HEAP_ARENA_SIZE 0x100000u

static uint8_t heap_arena[HEAP_ARENA_SIZE] __attribute__((aligned(4096)));
static size_t arena_next;

static void* static_alloc_pages(void* ctx, size_t count) {
  (void)ctx;
  size_t bytes = count * HEAP_PAGE_SIZE;
  if (arena_next + bytes > HEAP_ARENA_SIZE) {
    serial_puts(
        "[heap] static arena exhausted (1 MiB) — grow HEAP_ARENA_SIZE "
        "or land the R2 pmm provider\n");
    return NULL;
  }
  void* p = &heap_arena[arena_next];
  arena_next += bytes;
  return p;
}

static void static_free_pages(void* ctx, void* addr, size_t count) {
  (void)ctx;
  (void)addr;
  (void)count; /* no-op: the bump arena never returns pages */
}

void kernel_heap_init(void) {
  static const heap_provider_t static_provider = {
      .alloc_pages = static_alloc_pages,
      .free_pages = static_free_pages,
  };

  heap_init(&static_provider, NULL, (uintptr_t)heap_arena);
}
