/*
 * arch_timer.h — EL1 virtual generic timer (aarch64)
 *
 * The x86 analogue is the LAPIC timer in TSC-deadline mode: CNTV_CVAL is
 * an absolute deadline against the always-running virtual counter CNTVCT
 * (the TSC), not a reload interval — the tick handler re-arms by adding
 * the period to the previous deadline. Named arch_timer (not timer)
 * because the copied kernel/intr/timer.c from ToyOS64 owns that name
 * when the scheduler port lands.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_ARCH_TIMER_H
#define TOYOS_ARCH_AARCH64_ARCH_TIMER_H

#include <toyos/kernel/types.h>

/* Virtual timer = PPI 11 → INTID 27 (from the /timer node: GIC_PPI 11,
 * LEVEL_HIGH). The virtual timer is the one an EL1 kernel owns outright
 * — the physical timers belong to EL2/EL3 firmware. */
#define ARCH_TIMER_VIRT_PPI 11
#define ARCH_TIMER_VIRT_INTID (16 + ARCH_TIMER_VIRT_PPI)

/* Read CNTFRQ_EL0, announce it, bind the tick handler. Returns the
 * counter frequency in Hz. Does not start the timer. */
uint32_t arch_timer_init(void);

/* Arm the timer at hz ticks per second and zero the tick counter. */
void arch_timer_start(uint32_t hz);

/* Mask the timer interrupt (IMASK=1; a later start resumes cleanly). */
void arch_timer_stop(void);

/* Ticks taken since arch_timer_start; bumped from IRQ context. */
uint64_t arch_timer_ticks(void);

/* Raw virtual counter (CNTVCT_EL0) — monotonic; frequency from init. */
uint64_t arch_timer_counter(void);

#endif /* TOYOS_ARCH_AARCH64_ARCH_TIMER_H */
