/*
 * kstack.c — Kernel thread stack allocator (R1 static-region shim)
 *
 * Same allocation protocol as ToyOS64's kstack.c — bump + free-list over a
 * dedicated region, one guard page below each stack's usable pages, the
 * free-list node parked in a free slot's first usable page — but the region
 * is a static array: with the MMU off (paging is R2) there is no pmm/vmm to
 * carve pages from and no way to leave the guard page unmapped, so an
 * overflow runs silently until R2 replaces this file with the upstream copy
 * wired to the real mm.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/kstack.h>
#include <toyos/kernel/list.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

/* 2 MiB static region: ~104 slots of the standard 16 KiB stack + guard. */
#define KSTACK_REGION_SIZE 0x200000u

static uint8_t kstack_region[KSTACK_REGION_SIZE] __attribute__((aligned(4096)));
static uintptr_t kstack_next;
static spinlock_t kstack_lock;
static struct list_node
    kstack_free_list; /* freed slots, reused before bumping */

void kstack_init(void) {
  kstack_next = (uintptr_t)kstack_region;
  spin_lock_init(&kstack_lock);
  list_init(&kstack_free_list);

  serial_puts("[kstack] static region @ ");
  serial_print_hex((uint64_t)kstack_region);
  serial_puts(
      ", size 0x200000 (2 MiB, R1 shim — real allocator lands with R2 mm)\n");
}

void* kstack_alloc(size_t npages) {
  size_t total_pages = KSTACK_GUARD_PAGES + npages;
  uintptr_t bytes = total_pages * PAGE_SIZE;

  spin_lock_irqsave(&kstack_lock);
  /* Reuse a freed slot if available. All R1 stacks are THREAD_STACK_PAGES
   * (thread_alloc is the only caller). The list_node lives in the slot's
   * first usable page (base + guard), empty while the slot is free (the
   * stack grows down from the top). */
  if (!list_empty(&kstack_free_list)) {
    struct list_node* n = kstack_free_list.next;
    list_remove(n);
    spin_unlock_irqrestore(&kstack_lock);
    uintptr_t reuse = (uintptr_t)n - KSTACK_GUARD_PAGES * PAGE_SIZE;
    return (void*)reuse;
  }
  if (kstack_next + bytes > (uintptr_t)kstack_region + KSTACK_REGION_SIZE) {
    spin_unlock_irqrestore(&kstack_lock);
    return NULL;
  }
  uintptr_t slot = kstack_next;
  kstack_next += bytes;
  spin_unlock_irqrestore(&kstack_lock);
  return (void*)slot;
}

void kstack_free(void* base) {
  if (!base) return; /* boot thread's boot stack is not ours to free */

  /* Park the free-list node in the slot's first usable page (above the
   * guard slot) — the same trick as upstream, so kstack_alloc's reuse
   * path recovers the base with the same arithmetic. */
  struct list_node* n =
      (struct list_node*)((uintptr_t)base + KSTACK_GUARD_PAGES * PAGE_SIZE);

  spin_lock_irqsave(&kstack_lock);
  list_push_back(&kstack_free_list, n);
  spin_unlock_irqrestore(&kstack_lock);
}
