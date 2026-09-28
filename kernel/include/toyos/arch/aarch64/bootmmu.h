/*
 * bootmmu.h — boot-time MMU bring-up: identity trampoline + kernel tree
 *
 * R2.2 shape: TTBR0 = identity trampoline (the still-executing physical
 * window), TTBR1 = the real kernel tree (direct map + image + reserved
 * kstack/heap slots, layout in kva.h). bootmmu_init enables both;
 * bootmmu_to_high (entry.S) performs the absolute jump to the link VA.
 * The trampoline is per-core-dropped later (bootmmu_drop_trampoline).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_BOOTMMU_H
#define TOYOS_ARCH_AARCH64_BOOTMMU_H

#include <toyos/kernel/memmap.h>
#include <toyos/kernel/types.h>

/* Build both trees and enable the MMU + caches. Call once from kmain_low,
 * as early as possible (before any atomic/exclusive instruction — the
 * first spinlock will do). Memory discovery (memmap_init) must run first:
 * this runs pre-MMU and the DTB walk is plain loads, which is exactly why
 * the ordering works. Returns still executing at the physical base — the
 * caller then transfers to the link VA via bootmmu_to_high(). */
void bootmmu_init(const mem_region_t* banks, size_t nbanks);

/* AP counterpart (first C call of secondary_entry, MMU off): load BOTH
 * TTBRs off the shared static tables and enable this core's MMU + caches.
 * SCTLR/TCR/TTBR are per-core, so each released core must run its own
 * enable before its first atomic (MMU-off memory is all-Device;
 * exclusives fault there). Runs low under the trampoline. */
void bootmmu_ap_enable(void);

/* Drop this core's identity trampoline: TTBR0 → an empty root + full
 * local TLB flush. Valid only after mmio_window_flip() (device access
 * moves to the direct map, which TTBR1 serves on every core). */
void bootmmu_drop_trampoline(void);

/* entry.S: absolute transfer to the link VA after bootmmu_init — re-arms
 * VBAR and moves SP to their link VAs, then br to kmain_high (whose link
 * address the asm takes from its own literal pool — a C-passed function
 * pointer would be position-relative and land on the low copy). @arg
 * reaches kmain_high as its x0 argument. */
void bootmmu_to_high(uintptr_t arg);

#endif /* TOYOS_ARCH_AARCH64_BOOTMMU_H */
