/*
 * kstack.h — Kernel thread stack allocator
 *
 * Same API as ToyOS64's kstack.h. R1 deviation: with the MMU still off
 * (paging is R2) a slot's guard page is ordinary RAM — the layout and
 * sizes match upstream exactly (guard slot + usable pages, free-list
 * reuse), but an overflow can only be caught once pages can be left
 * unmapped. kernel/mm/kstack.c notes where that lands.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_KSTACK_H
#define TOYOS_KERNEL_KSTACK_H

#include <stddef.h>
#include <toyos/kernel/types.h>

/* One guard page sits below each stack's usable pages (a real hole from
 * R2 on; reserved slot space in R1). */
#define KSTACK_GUARD_PAGES 1

/* PAGE_SIZE lives here until the mm port (R2) owns it, matching upstream's
 * pmm.h placement. */
#define PAGE_SIZE 4096

/*
 * kstack_init - Initialise the allocator.
 *
 * Called once from kmain before sched_init().
 */
void kstack_init(void);

/*
 * kstack_alloc - Allocate a kernel thread stack of @npages usable pages plus
 *                one guard page.
 *
 * Returns the stack BASE (lowest virtual address). The guard page occupies
 * [base, base + PAGE_SIZE); the usable stack is
 * [base + PAGE_SIZE, base + (1+npages)*PAGE_SIZE), growing down from the top.
 * Returns NULL if the region is exhausted.
 *
 * The caller computes the initial stack pointer as:
 *   stack_top = base + (KSTACK_GUARD_PAGES + npages) * PAGE_SIZE
 */
void* kstack_alloc(size_t npages);

/*
 * kstack_free - Return a stack slot (from kstack_alloc) to the free-list for
 *               reuse. NULL is a safe no-op (boot thread's boot stack).
 *               Called from thread_release.
 */
void kstack_free(void* base);

#endif /* TOYOS_KERNEL_KSTACK_H */
