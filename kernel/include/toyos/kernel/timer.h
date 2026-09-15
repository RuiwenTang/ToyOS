/*
 * timer.h — kernel timer subsystem interface
 *
 * Same contract as ToyOS64's timer.h minus timer_lapic_ticks_per_ms(): on
 * aarch64 every core reads its own CNTFRQ_EL0, so the APs (next R1 slice)
 * arm their timers directly instead of borrowing a BSP-calibrated rate.
 * TIMER_HZ matches upstream.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_TIMER_H
#define TOYOS_KERNEL_TIMER_H

#include <toyos/kernel/types.h>

#define TIMER_HZ 1000 /* 1 ms tick */

/*
 * timer_init - Register the tick handler for the EL1 virtual timer's PPI
 *              and arm it at TIMER_HZ. The first tick fires only once
 *              interrupts are enabled (sched_start).
 */
void timer_init(void);

/* timer_get_ticks - Return ticks since the scheduler started (BSP-owned). */
uint64_t timer_get_ticks(void);

/* timer_get_ms - Return milliseconds since the scheduler started.
 *                TIMER_HZ=1000 so 1 tick == 1 ms. */
uint64_t timer_get_ms(void);

#endif /* TOYOS_KERNEL_TIMER_H */
