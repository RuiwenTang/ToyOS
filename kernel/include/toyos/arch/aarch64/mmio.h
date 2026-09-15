/*
 * mmio.h — 32-bit MMIO accessors (aarch64)
 *
 * Strictly 32-bit accesses: the RK3568's GIC rejects wider ones, and
 * 32-bit works everywhere (the blueprint's ITS-quirk note). R1 runs
 * identity-mapped physical, so a plain volatile cast is enough; paging
 * (R2) turns these into paddr→vaddr lookups.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_MMIO_H
#define TOYOS_ARCH_AARCH64_MMIO_H

#include <toyos/kernel/types.h>

static inline uint32_t mmio_read32(uintptr_t addr) {
  return *(volatile uint32_t*)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
  *(volatile uint32_t*)addr = val;
}

#endif /* TOYOS_ARCH_AARCH64_MMIO_H */
