/*
 * timer.c — EL1 virtual timer tick (aarch64)
 *
 * ToyOS64's timer.c is LAPIC one-shot + PIT calibration; the aarch64
 * counterpart keeps the tick discipline verbatim and drops only the
 * calibration (CNTFRQ_EL0 is exact, read at runtime — it differs per boot
 * path, 24 MHz under HVF -kernel vs 1 GHz under TCG/U-Boot) and the
 * LAPIC/PIT plumbing (arch_timer.c owns CNTV_*).
 *
 * Tick discipline (same order as upstream, all of it load-bearing):
 *   1. re-arm first — the deadline advances even if we switch away below;
 *   2. EOI before schedule() — a context switch never returns through the
 *      IRQ epilogue (switch_to's ret jumps to the new thread), so a
 *      deferred EOI would never run;
 *   3. gate on g_sched_started — below scheduler start, no switching;
 *   4. BSP-only timekeeping + sleep/alarm sweeps (single owner);
 *   5. schedule() unconditionally — every tick preempts.
 *
 * alarm_check and signal_deliver_on_interrupt are deferred until the
 * process/signal ports (R2/R3) land; the call sites return with them.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/kernel/atomic.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/timer.h>

static volatile uint64_t system_ticks;

static void timer_tick(uint32_t intid) {
  /* 1. One-shot mode: re-arm before anything else so the next tick is
   *    queued even if we context-switch away below. */
  arch_timer_reload();

  /* 2. EOI BEFORE schedule(): a context switch may not return through the
   *    IRQ epilogue (switch_to's ret jumps to the new thread), so a
   *    deferred EOI would never run. */
  gicv3_eoi(intid);

  /* 3. Below scheduler start, no context switching yet. */
  if (!__atomic_load_n(&g_sched_started, __ATOMIC_ACQUIRE)) return;

  /* 4. Global timekeeping + timed-wait sweeps are BSP-owned: every core
   *    drives its own schedule(), but only the BSP advances system_ticks
   *    and scans the sleep queue (single owner avoids a multi-core race). */
  if (this_cpu()->index == 0) {
    system_ticks++;

    if (system_ticks <= 5) {
      serial_printf("[Timer] tick #%u\n", system_ticks);
    }
    sleep_check_wakeups(
        system_ticks); /* wake expired nanosleep / futex-timeout */
    /* alarm_check(system_ticks) — fires SIGALRM; returns with the R2
     * process port. */
    /* signal_deliver_on_interrupt — catches user threads spinning in
     * ring 3; returns with the R3 signal port (needs the interrupted
     * frame's SPSR.EL to test user mode). */
  }

  /* 5. Every tick preempts. */
  schedule();
}

void timer_init(void) {
  serial_puts("[Timer] generic timer @ ");
  serial_print_dec(TIMER_HZ);
  serial_puts(" Hz (one-shot + reload)\n");

  gicv3_register_handler(ARCH_TIMER_VIRT_INTID, timer_tick);
  gicv3_enable_intid(ARCH_TIMER_VIRT_INTID);
  arch_timer_start(TIMER_HZ);
  /* First tick fires once IRQs are unmasked in sched_start(). */
}

uint64_t timer_get_ticks(void) { return system_ticks; }

uint64_t timer_get_ms(void) {
  /* TIMER_HZ=1000 → 1 tick == 1 ms. */
  return system_ticks;
}
