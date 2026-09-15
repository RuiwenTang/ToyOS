/*
 * cpu.h — aarch64 CPU-level helpers (exception level, DAIF, halt)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_CPU_H
#define TOYOS_ARCH_AARCH64_CPU_H

#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/types.h>

/* CurrentEL is bits [3:2]; report the numeric level (1, 2 or 3). */
static inline uint64_t current_el(void) {
  uint64_t el;
  __asm__ __volatile__("mrs %0, CurrentEL" : "=r"(el));
  return (el >> 2) & 3;
}

/* Unmask/mask IRQs at the core (DAIF.I ≈ RFLAGS.IF, daifclr ≈ sti).
 * FIQ stays masked — nothing in the design is Group 0. */
static inline void irq_enable(void) {
  __asm__ __volatile__("msr daifclr, #2" ::: "memory");
}

static inline void irq_disable(void) {
  __asm__ __volatile__("msr daifset, #2" ::: "memory");
}

/* This core's affinity id: {Aff3,Aff2,Aff1,Aff0} in bits [39:32]/[23:0]
 * (MT/U in [31:24] excluded). x86: the LAPIC id, but hierarchical — the
 * GICR match below is by these fields, not position. */
static inline uint64_t read_mpidr(void) { return sysreg_read(MPIDR_EL1); }

/* Sleep until the next interrupt; retires when it is taken, so execution
 * resumes after the handler returns through eret. */
static inline void cpu_wait_for_interrupt(void) { __asm__ __volatile__("wfi"); }

/* Park the core: interrupts off, wait forever. */
static inline void cpu_halt(void) {
  __asm__ __volatile__(
      "msr daifset, #0xf\n"
      "1: wfi\n"
      "b 1b\n");
}

#endif /* TOYOS_ARCH_AARCH64_CPU_H */
