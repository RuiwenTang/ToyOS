/*
 * sched_arch.h — arch state to switch (or not) inside schedule() (aarch64)
 *
 * ToyOS64's schedule() switches CR3 (address space), FS_BASE (user TLS) and
 * TSS.RSP0 (ring-3 entry stack) inline between the run-queue update and
 * switch_to. On aarch64 R1 all three are structurally absent: no paging
 * (one identity-mapped kernel space until R2 — TTBR0_EL1 takes CR3's role,
 * plus the ASID/TLBI plan from the blueprint), no user threads (user TLS is
 * TPIDR_EL0, R2+), and no TSS (SP_EL1 is each thread's own stack; the SVC
 * entry-stack question lands with ring 3).
 *
 * The hook keeps schedule()'s structure identical to upstream so the R2
 * diff is "fill in this function": load next's page-table root + ASID, set
 * TPIDR_EL0 for user TLS, in the same slot where the x86 inline asm sat.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_SCHED_ARCH_H
#define TOYOS_ARCH_AARCH64_SCHED_ARCH_H

#include <toyos/kernel/types.h>

struct thread;

/* R1: kernel threads only, single address space, no user state — nothing
 * to switch. fpu_context_switch (fpu.h) stays a separate call site, as
 * upstream has it. */
static inline void arch_sched_switch_state(struct thread* prev,
                                           struct thread* next) {
  (void)prev;
  (void)next;
}

#endif /* TOYOS_ARCH_AARCH64_SCHED_ARCH_H */
