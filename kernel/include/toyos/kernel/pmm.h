/*
 * pmm.h — Physical Memory Manager interface
 *
 * Bitmap-based allocator for 4 KiB physical pages.
 * Consumes the normalized memmap regions (upstream ate a Limine memmap).
 *
 * Ported from ToyOS64 (BSD-3 relicense, sole author).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_PMM_H
#define TOYOS_KERNEL_PMM_H

#include <toyos/kernel/memmap.h>
#include <toyos/kernel/types.h>

/* Page constants */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

/*
 * pmm_init - Initialize the physical memory manager
 *
 * @usable: normalized usable regions from memmap (page-aligned, sorted)
 * @n:      region count
 *
 * Places the bitmap and the per-frame refcount table in the first region
 * that fits, marks their own pages used, and counts free pages. Must run
 * with the MMU on (the pmm lock is a spinlock) and before any dynamic
 * allocation.
 */
void pmm_init(const mem_region_t* usable, size_t n);

/*
 * pmm_alloc - Allocate a single 4 KiB physical page
 *
 * Returns the physical address of the allocated page, or 0 on failure.
 */
uintptr_t pmm_alloc(void);

/*
 * pmm_free - Free a single 4 KiB physical page
 *
 * @phys: Physical address previously returned by pmm_alloc (must be
 *        page-aligned)
 */
void pmm_free(uintptr_t phys);

/*
 * pmm_is_managed - Is phys a pmm-managed RAM frame?
 *
 * Returns true for physical addresses inside the pmm bitmap's range
 * [0, max_phys). MMIO/device-memory mappings and any high memory are NOT
 * managed and must not be passed to pmm_free — aspace_destroy uses this to
 * skip them when tearing down an address space that mapped device memory.
 */
bool pmm_is_managed(uintptr_t phys);

/*
 * Physical page reference counting — backs Copy-on-Write fork.
 *
 * pmm_alloc() initialises a freshly allocated frame's count to 1. fork()
 * shares the frame between parent and child (inc); a CoW page-fault that
 * copies the frame, and address-space teardown, each drop their reference
 * (dec). The frame is only returned to the bitmap when the count reaches
 * 0. Intermediate page table pages are process-private and bypass this
 * (freed directly by pmm_free).
 */
void pmm_refcount_inc(uintptr_t phys);
uint32_t pmm_refcount_dec(uintptr_t phys); /* returns the new count */
uint32_t pmm_refcount_get(uintptr_t phys);

/*
 * pmm_alloc_pages - Allocate N contiguous 4 KiB physical pages
 *
 * @count: Number of contiguous pages to allocate
 *
 * Returns the physical address of the first page, or 0 on failure.
 */
uintptr_t pmm_alloc_pages(size_t count);

/*
 * pmm_free_pages - Free N contiguous 4 KiB physical pages
 *
 * @phys:  Physical address of the first page
 * @count: Number of pages to free
 */
void pmm_free_pages(uintptr_t phys, size_t count);

/*
 * pmm_total_pages - Total number of physical pages managed
 */
size_t pmm_total_pages(void);

/*
 * pmm_free_page_count - Number of currently free pages
 */
size_t pmm_free_page_count(void);

/*
 * PA<->VA seam. R2.1 runs the kernel identity-mapped, so these are the
 * identity; R2.2's higher-half move re-points them at the direct-map
 * offset and nothing else in pmm/heap changes.
 */
static inline void* pmm_phys_to_virt(uintptr_t pa) {
  return (void*)pa;
}

static inline uintptr_t pmm_virt_to_phys(void* va) {
  return (uintptr_t)va;
}

#endif /* TOYOS_KERNEL_PMM_H */
