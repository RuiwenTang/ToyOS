/*
 * percpu.c — Per-CPU data initialization (aarch64)
 *
 * Bind the BSP to cpu_locals[0] and point TPIDR_EL1 at it (the GS_BASE
 * role). PSCI bring-up (smp.c) releases cores 1..N-1 into
 * percpu_init_ap, which re-points TPIDR_EL1 only — the slots themselves
 * are ALL initialised here, on the BSP, before anything can enqueue to
 * them (cpu_locals[i].run_queue/run_lock must be valid before the first
 * cross-CPU thread_create_on, and sched_init later fills .idle).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/smp.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/serial.h>

struct cpu_local cpu_locals[MAX_CPUS];
unsigned ncpus = 1;

static void cpu_local_init(struct cpu_local* c, uint32_t me) {
  c->self = c; /* this_cpu() sanity check: this_cpu()->self == this_cpu() */
  c->index = me;
  c->current = NULL;     /* sched_init()/secondary_main set this */
  c->idle = NULL;        /* sched_init() seeds this */
  c->switch_prev = NULL; /* no thread reaped until the first switch-out */
  c->mpidr = 0;          /* smp_cpu_mpidr() fills it below */
  list_init(&c->run_queue);
  spin_lock_init(&c->run_lock); /* protects run_queue + switch */
}

void percpu_init(void) {
  unsigned n = smp_cpu_count();
  if (n == 0 || n > MAX_CPUS) n = 1;

  for (unsigned i = 0; i < n; i++) {
    cpu_local_init(&cpu_locals[i], i);
    cpu_locals[i].mpidr = smp_cpu_mpidr(i); /* /cpus affinity (slot 0 = BSP) */
  }

  sysreg_write(TPIDR_EL1, (uint64_t)(uintptr_t)&cpu_locals[0]);
  ncpus = n;

  serial_puts("[PerCPU] BSP = slot 0 (mpidr ");
  serial_print_hex(cpu_locals[0].mpidr);
  serial_puts("), ");
  serial_print_dec(n);
  serial_puts(" cpu slots initialized\n");
}

void percpu_init_ap(uint32_t me) {
  /* TPIDR_EL1 only: the slot was initialised by percpu_init() on the BSP,
   * and sched_init() has since filled cpu_locals[me].idle — re-running
   * cpu_local_init here would wipe both. The self-pointer sanity check
   * catches a wrong slot index immediately. */
  struct cpu_local* c = &cpu_locals[me];
  if (c->self != c) {
    serial_puts("[PerCPU] AP slot sanity check FAILED for core ");
    serial_print_dec(me);
    serial_puts("\n");
    cpu_halt();
  }
  sysreg_write(TPIDR_EL1, (uint64_t)(uintptr_t)c);
}
