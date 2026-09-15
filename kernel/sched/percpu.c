/*
 * percpu.c — Per-CPU data initialization (aarch64)
 *
 * Bind the BSP to cpu_locals[0] and point TPIDR_EL1 at it (the GS_BASE
 * role). PSCI bring-up (next R1 slice) binds slots 1..N-1 the same way
 * via percpu_init_ap.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/serial.h>

struct cpu_local cpu_locals[MAX_CPUS];
unsigned ncpus = 1;

static void cpu_local_init(struct cpu_local* c, uint32_t me) {
  c->self = c; /* this_cpu() sanity check: this_cpu()->self == this_cpu() */
  c->index = me;
  c->current = NULL;     /* sched_init() runs later */
  c->idle = NULL;        /* sched_init() seeds this */
  c->switch_prev = NULL; /* no thread reaped until the first switch-out */
  list_init(&c->run_queue);
  spin_lock_init(&c->run_lock); /* protects run_queue + switch */
}

void percpu_init(void) {
  struct cpu_local* c = &cpu_locals[0];
  cpu_local_init(c, 0);
  c->mpidr = read_mpidr(); /* the GICR_TYPER affinity match key */

  sysreg_write(TPIDR_EL1, (uint64_t)(uintptr_t)c);
  ncpus = 1;

  serial_puts("[PerCPU] BSP initialized: mpidr=");
  serial_print_hex(c->mpidr);
  serial_puts(", index=0\n");
}

void percpu_init_ap(uint32_t me) {
  struct cpu_local* c = &cpu_locals[me];
  cpu_local_init(c, me);
  /* mpidr was pre-filled by the BSP-side PSCI bring-up. */

  sysreg_write(TPIDR_EL1, (uint64_t)(uintptr_t)c);
}
