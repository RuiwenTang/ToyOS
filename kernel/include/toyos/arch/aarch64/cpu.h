/*
 * cpu.h — aarch64 CPU-level helpers (exception level, halt)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_CPU_H
#define TOYOS_ARCH_AARCH64_CPU_H

#include <toyos/kernel/types.h>

/* CurrentEL is bits [3:2]; report the numeric level (1, 2 or 3). */
static inline uint64_t current_el(void) {
  uint64_t el;
  __asm__ __volatile__("mrs %0, CurrentEL" : "=r"(el));
  return (el >> 2) & 3;
}

/* Park the core: interrupts off, wait forever. */
static inline void cpu_halt(void) {
  __asm__ __volatile__(
      "msr daifset, #0xf\n"
      "1: wfi\n"
      "b 1b\n");
}

#endif /* TOYOS_ARCH_AARCH64_CPU_H */
