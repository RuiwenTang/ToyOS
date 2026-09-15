/*
 * heap.h — Kernel heap allocator interface
 *
 * Linked-list free-block allocator with boundary tags for coalescing.
 * Uses a provider interface for page allocation to enable host-side testing.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_HEAP_H
#define TOYOS_KERNEL_HEAP_H

#include <toyos/kernel/types.h>

/* --- Page size (must match pmm.h) --- */

#define HEAP_PAGE_SIZE 4096

/* --- Provider interface --- */

/*
 * heap_provider_t — Callback interface for page-level memory operations.
 *
 * The kernel provider uses PMM + VMM. The test provider uses host malloc.
 * This indirection enables host-side unit testing of the core allocator.
 */
typedef struct heap_provider {
  /*
   * alloc_pages - Allocate and map 'count' contiguous 4 KiB pages.
   *
   * @ctx:   Provider-specific context
   * @count: Number of 4096-byte pages to allocate
   *
   * Returns the virtual address of the first page, or NULL on failure.
   */
  void* (*alloc_pages)(void* ctx, size_t count);

  /*
   * free_pages - Free previously allocated pages.
   *
   * @ctx:   Provider-specific context
   * @addr:  Virtual address previously returned by alloc_pages
   * @count: Number of pages to free
   */
  void (*free_pages)(void* ctx, void* addr, size_t count);
} heap_provider_t;

/* --- Public API --- */

/*
 * heap_init - Initialize the heap with a given provider
 *
 * @provider:      Page allocation callbacks
 * @provider_ctx:  Opaque context passed to provider callbacks (NULL for kernel)
 * @start_addr:    Virtual address where the heap region begins
 *                 (e.g. 0xFFFFFFFF40000000 for kernel)
 *
 * No pages are allocated until the first kmalloc call (lazy growth).
 */
void heap_init(const heap_provider_t* provider, void* provider_ctx,
               uintptr_t start_addr);

/*
 * kmalloc - Allocate 'size' bytes of memory (16-byte aligned)
 *
 * Returns a pointer to the allocated memory, or NULL on failure.
 * Passing size == 0 returns NULL.
 */
void* kmalloc(size_t size);

/*
 * kfree - Free a previously allocated block
 *
 * @ptr: Pointer returned by kmalloc/krealloc/kcalloc, or NULL (safe no-op)
 */
void kfree(void* ptr);

/*
 * kfree_sized - Free a block with a size hint
 *
 * Currently equivalent to kfree(). The size hint is reserved for future
 * slab-cache optimizations.
 */
void kfree_sized(void* ptr, size_t size);

/*
 * kcalloc - Allocate zero-initialized memory for an array
 *
 * @nmemb: Number of elements
 * @size:  Size of each element
 *
 * Returns zero-filled memory, or NULL on overflow or OOM.
 */
void* kcalloc(size_t nmemb, size_t size);

/*
 * krealloc - Resize a previously allocated block
 *
 * @ptr:  Existing allocation (or NULL to behave like kmalloc)
 * @size: New requested size
 *
 * Returns pointer to resized memory (may differ from ptr), or NULL on failure.
 * If size is 0, frees ptr and returns NULL.
 */
void* krealloc(void* ptr, size_t size);

/*
 * heap_check_live_alloc - LOCK-FREE integrity check of a known-live USED
 * block's envelope (header/footer/tail-redzone). Returns 1 if intact, 0 if
 * corrupted. Mirrors heap_check() but returns instead of halting, so the
 * scheduler can print the offending thread's context first.
 *
 * Does NOT take heap_lock. A USED block's metadata is written only at
 * alloc/free, never by another core's kmalloc/kfree on a different block
 * (coalescing touches only the freed neighbour's header/footer). This closes
 * the blind spot where a thread's stack block is never freed during the
 * thread's lifetime, so its tail redzone — sitting at stack_top, right above
 * the parked save-slot's ret word — is otherwise never checked.
 *
 * Precondition: user_ptr is a currently-allocated (USED) block that is not
 * being concurrently freed.
 */
int heap_check_live_alloc(void* user_ptr);

/*
 * heap_dump_stats - Print heap statistics (debug)
 *
 * Outputs heap usage information via the debug output mechanism.
 */
void heap_dump_stats(void);

/* --- Kernel-specific initialization --- */

/*
 * kernel_heap_init - Initialize the kernel heap with PMM + VMM provider
 *
 * Convenience function that creates the kernel provider and calls heap_init()
 * with the kernel heap address (0xFFFFFFFF40000000).
 */
void kernel_heap_init(void);

#endif /* TOYOS_KERNEL_HEAP_H */
