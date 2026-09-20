/*
 * bootmmu.h — boot-time identity map (aarch64)
 *
 * Turns the MMU on with two static tables so RAM becomes Normal Cacheable
 * (atomics/caches are architectural no-ops on Device memory, which is all
 * you get with the MMU off). This is scaffolding for R1 — the R2 paging
 * port replaces the programming wholesale; nothing here is part of that
 * design. See bootmmu.c for the full rationale.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_BOOTMMU_H
#define TOYOS_ARCH_AARCH64_BOOTMMU_H

/* Build the identity tables and enable the MMU + caches. Call once, as
 * early in kmain as possible (before any atomic/exclusive instruction —
 * the first spinlock will do). Identity map: no address changes. */
void bootmmu_init(void);

/* AP counterpart (first C call of secondary_entry): enable THIS core's
 * MMU + caches off the same shared tables. SCTLR/TCR/TTBR are per-core,
 * so each released core must run its own enable before its first atomic
 * (MMU-off memory is all-Device; exclusives fault there). */
void bootmmu_ap_enable(void);

#endif /* TOYOS_ARCH_AARCH64_BOOTMMU_H */
