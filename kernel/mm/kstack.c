/*
 * kstack.c — Kernel thread stack allocator (dedicated pages + guard page)
 *
 * Ported from ToyOS64 (BSD-3 relicense, sole author): bump allocator over
 * a fixed kernel-VA region; each allocation is [guard page (unmapped)]
 * [npages usable RW pages]. The guard page catches downward stack overflow
 * as an immediate synchronous abort (invalid L3 entry), and traps.c turns
 * that into a thread kill.
 *
 * The region lives in TTBR1 slot 511 / L1 #508 (kva.h), which the direct
 * map (slot 256) can never cover, so the guard's unmapped entry is never
 * bypassed by the all-physical alias — the aarch64 restatement of
 * upstream's "PML4 entry 511 vs HHDM entry 256" note. Unlike R1's static
 * shim, pages are real: pmm frames mapped 4 KiB at a time by the kernel
 * tree walker.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/kva.h>
#include <toyos/arch/aarch64/paging.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/list.h>
#include <toyos/kernel/pmm.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

#define KSTACK_REGION_START KERNEL_KSTACK_BASE
#define KSTACK_REGION_SIZE KERNEL_KSTACK_SIZE

static uintptr_t kstack_next;
static spinlock_t kstack_lock;
static struct list_node
    kstack_free_list; /* freed slots, reused before bumping */

void kstack_init(void) {
  kstack_next = KSTACK_REGION_START;
  spin_lock_init(&kstack_lock);
  list_init(&kstack_free_list);

  serial_puts("[kstack] region @ ");
  serial_print_hex(KSTACK_REGION_START);
  serial_puts(", size ");
  serial_print_hex(KSTACK_REGION_SIZE);
  serial_puts(" (guarded slots, guard page unmapped)\n");
}

void* kstack_alloc(size_t npages) {
  size_t total_pages = KSTACK_GUARD_PAGES + npages;
  uintptr_t bytes = total_pages * PAGE_SIZE;

  spin_lock_irqsave(&kstack_lock);
  /* Reuse a freed slot if available. All stacks are THREAD_STACK_PAGES
   * (thread_alloc is the only caller); a freed slot's pages stay mapped,
   * so reuse needs no pmm_alloc/kernel_map_page. The list_node lives in
   * the slot's first usable page (base + guard), empty while the slot is
   * free (the stack grows down from the top). */
  if (!list_empty(&kstack_free_list)) {
    struct list_node* n = kstack_free_list.next;
    list_remove(n);
    spin_unlock_irqrestore(&kstack_lock);
    uintptr_t reuse = (uintptr_t)n - KSTACK_GUARD_PAGES * PAGE_SIZE;
    return (void*)reuse;
  }
  if (kstack_next + bytes > KSTACK_REGION_START + KSTACK_REGION_SIZE) {
    spin_unlock_irqrestore(&kstack_lock);
    serial_puts("[kstack] FATAL: kstack region exhausted\n");
    return NULL;
  }
  uintptr_t base = kstack_next;
  kstack_next += bytes;
  spin_unlock_irqrestore(&kstack_lock);

  /* Map the usable pages (guard page deliberately left unmapped). */
  for (size_t i = 0; i < npages; i++) {
    uintptr_t phys = pmm_alloc();
    if (!phys) {
      serial_puts("[kstack] FATAL: out of physical pages\n");
      return NULL;
    }
    uintptr_t virt = base + (KSTACK_GUARD_PAGES + i) * PAGE_SIZE;
    if (kernel_map_page(virt, phys, PTE_KERNEL_DATA_FLAGS) < 0) {
      serial_puts("[kstack] FATAL: kernel_map_page failed\n");
      return NULL;
    }
  }

  return (void*)base;
}

void kstack_free(void* base) {
  if (!base) return; /* boot thread's boot stack is not ours to free */
  /* Return the slot to the free-list. The list_node is stashed in the
   * slot's first usable page (base + guard) — unused while the slot is
   * free. Pages stay mapped and are reused verbatim by the next
   * kstack_alloc. */
  struct list_node* n =
      (struct list_node*)((uintptr_t)base + KSTACK_GUARD_PAGES * PAGE_SIZE);

  spin_lock_irqsave(&kstack_lock);
  list_push_back(&kstack_free_list, n);
  spin_unlock_irqrestore(&kstack_lock);
}
