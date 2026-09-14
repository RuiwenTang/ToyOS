/*
 * traps.c — exception dispatcher (aarch64)
 *
 * R1 scope: decode ESR, report, and park. No forwardable source exists yet
 * (GIC arrives next), so every IRQ/FIQ is unexpected by definition and
 * every sync trap is a kernel bug or a deliberate smoke test. The
 * dispatch-table structure below is the seam where the real handlers
 * (timer, GIC, SVC) will register — same shape as an x86 isr table fed
 * from the vector number.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/trap.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* ESR_EL1.EC — exception class, bits [31:26]. */
#define EC_UNKNOWN 0x00
#define EC_SVC64 0x15 /* SVC from AArch64, any EL */

static const char* class_name(uint64_t class) {
  switch (class) {
    case TRAP_SYNC_CUR_SP0:
      return "sync cur sp0";
    case TRAP_IRQ_CUR_SP0:
      return "irq cur sp0";
    case TRAP_FIQ_CUR_SP0:
      return "fiq cur sp0";
    case TRAP_SERROR_CUR_SP0:
      return "serror cur sp0";
    case TRAP_SYNC_CUR_SPX:
      return "sync cur spx";
    case TRAP_IRQ_CUR_SPX:
      return "irq cur spx";
    case TRAP_FIQ_CUR_SPX:
      return "fiq cur spx";
    case TRAP_SERROR_CUR_SPX:
      return "serror cur spx";
    case TRAP_SYNC_LOW_A64:
      return "sync low a64";
    case TRAP_IRQ_LOW_A64:
      return "irq low a64";
    case TRAP_FIQ_LOW_A64:
      return "fiq low a64";
    case TRAP_SERROR_LOW_A64:
      return "serror low a64";
    case TRAP_SYNC_LOW_A32:
      return "sync low a32";
    case TRAP_IRQ_LOW_A32:
      return "irq low a32";
    case TRAP_FIQ_LOW_A32:
      return "fiq low a32";
    case TRAP_SERROR_LOW_A32:
      return "serror low a32";
    default:
      return "?";
  }
}

static uint64_t read_esr(void) {
  uint64_t v;
  __asm__ __volatile__("mrs %0, esr_el1" : "=r"(v));
  return v;
}

static uint64_t read_far(void) {
  uint64_t v;
  __asm__ __volatile__("mrs %0, far_el1" : "=r"(v));
  return v;
}

static void trap_dump(const struct trap_frame* f, const char* why) {
  uint64_t esr = read_esr();

  serial_puts("\n!! exception: ");
  serial_puts(why);
  serial_puts(" (");
  serial_puts(class_name(f->class));
  serial_puts(")\n");
  serial_puts("   esr ");
  serial_print_hex(esr);
  serial_puts(" (EC 0x");
  serial_print_hex(esr >> 26);
  serial_puts("), elr ");
  serial_print_hex(f->elr);
  serial_puts(", spsr ");
  serial_print_hex(f->spsr);
  serial_puts(", far ");
  serial_print_hex(read_far());
  serial_puts("\n");
}

void trap_dispatch(struct trap_frame* f) {
  uint64_t esr = read_esr();
  uint64_t ec = esr >> 26;

  if (f->class == TRAP_SYNC_CUR_SPX && ec == EC_SVC64) {
    /* R1 smoke test: SVC at EL1 is the only intentional trap. Real SVC
     * entry for ring 3 (R2) takes the LOW_A64 path instead. */
    serial_puts("svc #");
    serial_print_dec(esr & 0xffff);
    serial_puts(" taken at ");
    serial_print_hex(f->elr);
    serial_puts(" — vector path OK\n");
    return;
  }

  trap_dump(f, "unexpected");
  cpu_halt(); /* no way out until timers/GIC exist */
}
