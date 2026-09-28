/*
 * smp.h — secondary-core bring-up + inter-processor interrupts (aarch64)
 *
 * The x86 analogue is the trampoline + INIT-SIPI-SIPI + SIPI-wait loop of
 * smp.c, plus the LAPIC ICR for reschedule IPIs. Here PSCI CPU_ON releases
 * each parked core into secondary_entry (entry.S), and IPIs are GICv3 SGIs
 * raised by an ICC_SGI1R_EL1 write (hardware-broadcast, no per-target
 * programming).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_SMP_H
#define TOYOS_ARCH_AARCH64_SMP_H

#include <toyos/kernel/types.h>

/* Probe /cpus (CPU count + per-slot MPIDR affinity) and initialise the PSCI
 * conduit from /psci. Idempotent-safe to call once from kmain before
 * percpu_init(); the slot table it builds is what percpu_init consumes.
 * Degrades to 1 core (loudly) when either lookup fails. */
void smp_probe(const void* dtb);

/* Number of CPUs found in /cpus (>= 1) and slot i's MPIDR affinity (the
 * CPU_ON target argument — DTB `reg` value, no MT/reserved bits). */
unsigned smp_cpu_count(void);
uint64_t smp_cpu_mpidr(unsigned i);

/* Register the SGI handlers (reschedule kick + echo) in the GIC dispatcher.
 * Must run before any core can receive an SGI: before bring-up on the BSP. */
void smp_ipi_init(void);

/* PSCI CPU_ON every secondary and wait (bounded) for its online handshake.
 * Prints the online count; degraded cores are skipped, not fatal. */
void smp_start_secondaries(void);

/* AP-side C entry (from secondary_entry in entry.S; never returns):
 * percpu bind, per-core GICR + timer, online handshake, then the idle
 * thread — mirroring sched_start's stack-swap discipline. */
void secondary_main(uint32_t me);

/* R1 acceptance ("IPI echo"): rounds of an all-but-self SGI, each AP
 * echoing back to the BSP. Runs in kmain with IRQs temporarily unmasked —
 * safe pre-sched_start because ticks below g_sched_started only EOI+reload. */
void smp_echo_test(void);

/* R2.2 acceptance: flip the device window to the direct map and drop the
 * identity trampoline on every core (BSP directly, APs via SGI). Call
 * only after all cores are online (the echo test proves it) — after this,
 * nothing below the link VA exists anywhere. */
void smp_drop_trampolines(void);

#endif /* TOYOS_ARCH_AARCH64_SMP_H */
