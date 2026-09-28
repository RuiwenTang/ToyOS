/*
 * paging.h — kernel-tree page-table walker (TTBR1, R2.2)
 *
 * The arch counterpart of ToyOS64's vmm map/unmap page: walks the static
 * kernel root (paging_kernel_root) at 4 KiB granularity, allocating
 * missing intermediate tables from the pmm. The full vmm.c port (user
 * trees, TTBR0, per-process) is R2.4; today's only client is kstack's
 * guarded slots.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_PAGING_H
#define TOYOS_ARCH_AARCH64_PAGING_H

#include <toyos/kernel/types.h>

/* PTE bits for kernel data pages (kstack): page descriptor (0b11 — at L3
 * the type field must be 0b11, a "block" bit pattern there is reserved
 * and the hardware reports a level-3 translation fault), AF,
 * inner-shareable, Normal WB, EL1 RW, PXN (data only). nG=0 — global,
 * matching the ASID-0 discipline. */
#define PTE_KERNEL_DATA_FLAGS \
  ((3u << 0) | (1u << 10) | (3u << 8) | (1ull << 2) | (1ull << 53))

/* Map one 4 KiB page into the kernel tree at @va → @pa.
 * Returns 0, or -1 (missing intermediate could not be allocated). */
int kernel_map_page(uintptr_t va, uintptr_t pa, uint64_t flags);

/* Unmap @va (4 KiB) from the kernel tree and invalidate the line
 * everywhere. Returns the mapped PA, or 0 if @va was unmapped. */
uintptr_t kernel_unmap_page(uintptr_t va);

#endif /* TOYOS_ARCH_AARCH64_PAGING_H */
