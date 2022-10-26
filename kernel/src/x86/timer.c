#include "x86/timer.h"

#include <stdint.h>

#include "kprintf.h"
#include "x86/io.h"
#include "x86/irq.h"

#define TIMER_IRQ 0
/* 8253/8254 PIT (Programmable Interval Timer) */
#define TIMER0 0x40     /* I/O port for timer channel 0 */
#define TIMER_MODE 0x43 /* I/O port for timer mode control */
#define RATE_GENERATOR \
  0x34  // 00-11-010-0 Counter0 - LSB then MSB - rate generator - binary

#define TIMER_FREQ 1193182L /* clock frequency for timer in PC and AT */
#define HZ 100              /* clock freq (software settable on IBM-PC) */

#define TIME_COUNT_DOWN 1000
volatile uint32_t g_timer_count_down = 0;

void timer_irq_handler(Registers* regs) {
  if (g_timer_count_down == 0) {
    g_timer_count_down = TIME_COUNT_DOWN;

    kprintf("Time count down refresh\n");
    return;
  }

  g_timer_count_down--;
}

void timer_init() {
  x86_outb(TIMER_MODE, RATE_GENERATOR);
  x86_iowait();

  x86_outb(TIMER0, (uint8_t)TIMER_FREQ / HZ);
  x86_iowait();

  x86_outb(TIMER0, (uint8_t)((TIMER_FREQ / HZ) >> 8));
  x86_iowait();

  irq_register_handler(TIMER_IRQ, &timer_irq_handler);
}
