/*
 * heap_provider.c — kernel heap provider: pmm-backed
 *
 * heap.c itself is the unmodified ToyOS64 copy and only talks to pages
 * through heap_provider_t. R2.1: pages come straight from the pmm through
 * the identity map (VA == PA), which is why this file has no vmm_map_page
 * calls yet — the R2.2 higher-half move swaps this provider body for the
 * upstream KERNEL_HEAP_VA one and nothing else changes.
 *
 * Contiguity note: heap_grow's extend-in-place path wants each new
 * allocation to abut the current heap end. While the heap is the only
 * runtime pmm client (true until R2.2's page tables) the bitmap's
 * lowest-free-first order guarantees exactly that — heap pages are never
 * returned to the pmm mid-life, so each grow lands directly above the
 * last. A non-adjacent return would still be handled by heap_grow's
 * new-free-block fallback.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/heap.h>
#include <toyos/kernel/pmm.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

static void* kernel_alloc_pages(void* ctx, size_t count) {
  (void)ctx;

  uintptr_t phys = pmm_alloc_pages(count);
  if (!phys) return NULL; /* pmm already printed if it was OOM-shaped */
  return pmm_phys_to_virt(phys);
}

static void kernel_free_pages(void* ctx, void* addr, size_t count) {
  (void)ctx;
  if (!addr) return;
  pmm_free_pages(pmm_virt_to_phys(addr), count);
}

void kernel_heap_init(void) {
  static const heap_provider_t pmm_provider = {
      .alloc_pages = kernel_alloc_pages,
      .free_pages = kernel_free_pages,
  };

  /* start_addr is nominal until the first grow fixes it to the actual
   * allocation address (heap.c sets heap_start when heap_size == 0). */
  heap_init(&pmm_provider, NULL, 0);

  size_t free_pages = pmm_free_page_count();
  serial_puts("[heap] provider: pmm (identity-mapped), ");
  serial_print_dec(free_pages);
  serial_puts(" free pages available\n");
}
