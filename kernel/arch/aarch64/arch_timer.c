/*
 * arch_timer.c — EL1 virtual generic timer (aarch64)
 *
 * Pure CNTV_* driver: frequency discovery, arming and re-arming. The tick
 * itself (timekeeping + schedule()) lives in kernel/intr/timer.c, which
 * owns the handler registration — this file no longer knows about the GIC
 * beyond the INTID in its header.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

#define CNTV_CTL_ENABLE (1u << 0)
#define CNTV_CTL_IMASK (1u << 1)

static uint64_t timer_freq;
static uint64_t tick_period;

uint32_t arch_timer_init(void) {
  timer_freq = sysreg_read(CNTFRQ_EL0);
  serial_puts("timer: CNTFRQ ");
  serial_print_dec(timer_freq);
  serial_puts(" Hz, virtual timer INTID 27\n");
  return (uint32_t)timer_freq;
}

void arch_timer_start(uint32_t hz) {
  tick_period = timer_freq / hz;

  /* Fresh deadline from now: a masked-but-expired deadline from an earlier
   * arm would fire the instant IRQs unmask. */
  sysreg_write(CNTV_CVAL_EL0, sysreg_read(CNTVCT_EL0) + tick_period);
  barrier_dsb_sy(); /* deadline lands before the timer is unmasked */
  sysreg_write(CNTV_CTL_EL0, CNTV_CTL_ENABLE);
  barrier_isb();
}

void arch_timer_reload(void) {
  /* Deadline += period (not now + period) so long-run frequency is exact.
   * Called as the first thing in the tick handler: the next deadline is
   * queued even if the handler context-switches away and never returns. */
  sysreg_write(CNTV_CVAL_EL0, sysreg_read(CNTV_CVAL_EL0) + tick_period);
  barrier_dsb_sy();
}

void arch_timer_stop(void) {
  sysreg_write(CNTV_CTL_EL0, CNTV_CTL_IMASK);
  barrier_isb();
}

uint64_t arch_timer_counter(void) { return sysreg_read(CNTVCT_EL0); }

uint64_t arch_timer_freq(void) { return timer_freq; }
