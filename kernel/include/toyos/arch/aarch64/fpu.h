/*
 * fpu.h — FPSIMD/NEON context switching (R1 stubs)
 *
 * Same three-function contract as ToyOS64's arch/x86_64/fpu.h (eager
 * switching — no lazy owner tracking), but empty for R1: the kernel is
 * compiled with -mgeneral-regs-only and CPACR_EL1 keeps FP/AdvSIMD
 * trapped, so no context in the system can hold FPSIMD state yet. The
 * kernel never executes a FP instruction and there are no user threads
 * until R2 — a no-op switch is semantically correct, and any accidental
 * FP use traps to the vector dispatcher instead of silently corrupting
 * nothing.
 *
 * R2 replaces this file with the real implementation: CPACR_EL1 enable,
 * 32 x 128-bit registers + FPCR/FPSR saved into an (enlarged) TCB area on
 * every switch.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_FPU_H
#define TOYOS_ARCH_AARCH64_FPU_H

struct thread;

/* Per-CPU enable. Real version sets CPACR_EL1.FPEN; stub keeps FP trapped. */
static inline void fpu_init(void) {}

/* Eager save/restore under run_lock — nothing to switch while no context
 * can own FPSIMD state. */
static inline void fpu_context_switch(struct thread* prev,
                                      struct thread* next) {
  (void)prev;
  (void)next;
}

/* No lazy owner to detach at exit (eager model keeps this a no-op even in
 * the real version, like upstream). */
static inline void fpu_on_thread_exit(struct thread* t) { (void)t; }

#endif /* TOYOS_ARCH_AARCH64_FPU_H */
