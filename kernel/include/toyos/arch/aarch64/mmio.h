/*
 * mmio.h — 32-bit MMIO accessors (aarch64)
 *
 * Strictly 32-bit accesses: the RK3568's GIC rejects wider ones, and
 * 32-bit works everywhere (the blueprint's ITS-quirk note). Device
 * registers are reached through mmio(): physical address + the live
 * device window — identity (offset 0) while the boot trampoline is up,
 * the direct map (+KERNEL_DM_BASE) after mmio_window_flip(). Both
 * windows are valid for every core between the flip and its own
 * trampoline drop, so the flip is a plain store with no rendezvous.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_MMIO_H
#define TOYOS_ARCH_AARCH64_MMIO_H

#include <toyos/arch/aarch64/kva.h>
#include <toyos/kernel/types.h>

/* Device VA window offset (0 = identity, KERNEL_DM_BASE = direct map).
 * Set to KERNEL_DM_BASE once, by mmio_window_flip(), before the first
 * core drops its trampoline. */
extern uint64_t mmio_va_offset;

static inline uintptr_t mmio(uintptr_t pa) { return pa + mmio_va_offset; }

static inline uint32_t mmio_read32(uintptr_t addr) {
  return *(volatile uint32_t*)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
  *(volatile uint32_t*)addr = val;
}

/* Flip the device window to the direct map. Call after every core runs
 * at the high VA (TTBR1 shared and on for all) and before any core
 * drops its identity trampoline. */
static inline void mmio_window_flip(void) { mmio_va_offset = KERNEL_DM_BASE; }

#endif /* TOYOS_ARCH_AARCH64_MMIO_H */
