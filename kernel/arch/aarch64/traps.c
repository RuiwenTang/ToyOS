/*
 * traps.c — exception dispatcher (aarch64)
 *
 * R1 scope: decode ESR, report, and park. EL1 IRQs now flow to the GIC
 * dispatcher (gicv3_irq_enter — ack, handler table, EOI); every sync
 * trap is still a kernel bug or a deliberate smoke test. The GIC's
 * handler table is where the copied kernel/intr/irq.c takes over when
 * the scheduler port lands.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/kva.h>
#include <toyos/arch/aarch64/trap.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* ESR_EL1.EC — exception class, bits [31:26]. */
#define EC_UNKNOWN 0x00
#define EC_SVC64 0x15 /* SVC from AArch64, any EL */
#define EC_DATA_ABORT_CUR                       \
  0x25 /* data abort from THIS EL (guard hits); \
        * 0x24 is the lower-EL variant */

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

volatile uint32_t kstack_guard_hits;

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

  if (f->class == TRAP_IRQ_CUR_SPX) {
    gicv3_irq_enter();
    return;
  }

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

  /* R2.2: kstack guard hit. A data abort whose FAR lands inside the
   * dedicated kstack VA region can only be a downward stack overflow —
   * the guard page is the only unmapped hole there, and the direct map
   * (which would alias the PA) is a different L0 slot. Kill the thread
   * instead of halting: the faulting frame is abandoned by thread_exit's
   * switch-away, and the killer never returns here. Unmask IRQs first —
   * exception entry masked them, we never ERET back through the frame,
   * and the next thread inherits PSTATE as-is (a masked core would never
   * take another tick and the scheduler would freeze mid-poll). */
  if (f->class == TRAP_SYNC_CUR_SPX && ec == EC_DATA_ABORT_CUR) {
    uint64_t far = read_far();
    if (far >= KERNEL_KSTACK_BASE &&
        far < KERNEL_KSTACK_BASE + KERNEL_KSTACK_SIZE) {
      kstack_guard_hits++;
      serial_puts("\nkernel: kstack GUARD HIT (far ");
      serial_print_hex(far);
      serial_puts(") — stack overflow, killing thread\n");
      irq_enable();
      thread_exit(); /* noreturn */
    }
  }

  trap_dump(f, "unexpected");
  cpu_halt(); /* kernel bug by definition — the park loop never returns */
}
