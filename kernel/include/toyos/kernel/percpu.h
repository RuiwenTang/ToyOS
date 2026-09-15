/*
 * percpu.h — Per-CPU data (aarch64)
 *
 * Each CPU owns a struct cpu_local. TPIDR_EL1 points at the current CPU's
 * cpu_local and this_cpu() reads it back — the exact GS_BASE pattern from
 * ToyOS64, minus its one wart: the %gs:0 self-indirection existed because
 * reading GS_BASE needs a serialising rdmsr, while TPIDR_EL1 is one MRS, so
 * here this_cpu() reads the base register directly.
 *
 * The struct is the aarch64 R1 subset of ToyOS64's cpu_local: the gdt/tss/
 * df_stack (no segmentation or IST on arm), syscall_rsp0/frame (SVC entry
 * is R2) and shootdown_ack (aarch64 TLBI is hardware-broadcast, no IPI
 * shootdown protocol) fields are gone; fpu_owner was already vestigial
 * upstream (eager FPU). R2+ adds back what its modules need.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_PERCPU_H
#define TOYOS_KERNEL_PERCPU_H

#include <toyos/kernel/list.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/types.h>

/* Forward declarations — used by pointer fields below. */
struct thread;

#define MAX_CPUS 16

/*
 * cpu_local - Per-CPU state.
 *
 * `self` is kept (set to the containing slot) as a cheap bring-up sanity
 * check: this_cpu()->self == this_cpu() catches a missing TPIDR_EL1 write
 * immediately, like the upstream %gs:0 self-check.
 */
struct cpu_local {
  struct cpu_local* self; /* back-pointer == this slot */
  uint64_t mpidr;         /* this CPU's MPIDR affinity (GICR match key) */
  uint32_t index;         /* 0..ncpus-1 */
  struct thread* current; /* running thread */
  struct thread* idle;    /* this CPU's idle thread */
  struct thread*
      switch_prev; /* last thread switched away on this core; reaped by the
                    * next schedule() once its refcount hits 0 (UAF fix) */
  struct list_node run_queue; /* this CPU's run queue */
  spinlock_t run_lock; /* protects run_queue + the pick/switch sequence; held
                        * across switch_to, released on the resume side */
};

extern struct cpu_local cpu_locals[MAX_CPUS];
extern unsigned ncpus;

/* --- Per-CPU access --- */

/*
 * this_cpu - Return the current CPU's cpu_local.
 *
 * Reads TPIDR_EL1 (set once by percpu_init/percpu_init_ap and never swapped
 * — there is no swapgs-equivalent hazard on arm). No serialisation needed.
 */
static inline struct cpu_local* this_cpu(void) {
  return (struct cpu_local*)sysreg_read(TPIDR_EL1);
}

/*
 * percpu_init - Bind the BSP to cpu_locals[0] and set TPIDR_EL1.
 *
 * Called from kmain() after gicv3_init() and before sched_init().
 */
void percpu_init(void);

/*
 * percpu_init_ap - Bind an AP to cpu_locals[me] and set TPIDR_EL1.
 *
 * @me: this CPU's slot index (1..ncpus-1). Called from the PSCI secondary
 *      entry (R1 next slice); mpidr is assumed pre-filled by the BSP-side
 *      bring-up. Unlike x86 there is no lgdt to clobber the base register
 *      on the way, so ordering relative to other per-CPU init is free.
 */
void percpu_init_ap(uint32_t me);

#endif /* TOYOS_KERNEL_PERCPU_H */
