/*
 * main.c — kernel C entry point (aarch64)
 *
 * entry.S has already settled the core to EL1, cleared .bss and set up the
 * boot stack; kmain receives the DTB pointer from the boot contract (x0).
 *
 * R1 scope: console, FDT walk, exception smoke (SVC), GICv3 + generic
 * timer bring-up, IRQ tick smoke. Scheduler + PSCI follow in the next R1
 * slice, per the roadmap in docs/aarch64-port-arch.md.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

#define SMOKE_TICKS 30

void kmain(const void* dtb) {
  uint32_t size;

  serial_init();
  serial_puts("\nToyOS aarch64 R1\n");
  serial_puts("boot EL: EL");
  serial_print_dec(current_el());
  serial_puts(", dtb @ ");
  serial_print_hex((uintptr_t)dtb);
  serial_puts("\n");

  size = fdt_valid(dtb);
  if (size == 0) {
    serial_puts("fdt: invalid blob, halting\n");
    cpu_halt();
  }
  serial_puts("fdt: blob size ");
  serial_print_dec(size);
  serial_puts("\n");

  fdt_node_t chosen = fdt_find_node(dtb, "/chosen");
  serial_puts("fdt: /chosen node @ ");
  serial_print_dec((uint32_t)chosen);
  serial_puts("\n");
  if (chosen >= 0) {
    const void* out;
    int len = fdt_get_prop(dtb, chosen, "stdout-path", &out);
    if (len >= 0) {
      serial_puts("fdt: stdout-path = ");
      serial_puts((const char*)out);
      serial_puts("\n");
    }
  }

  fdt_dump(dtb);

  serial_puts("\nR1: exception vector smoke test\n");
  __asm__ __volatile__("svc #0x42");
  serial_puts("R1: returned from SVC, context restore OK\n");

  serial_puts("\nR1: GICv3 + generic timer bring-up\n");
  if (gicv3_init(dtb) != 0) {
    serial_puts("gic: init failed, halting\n");
    cpu_halt();
  }
  uint64_t freq = arch_timer_init();
  arch_timer_start(10);
  gicv3_enable_intid(ARCH_TIMER_VIRT_INTID);

  serial_puts("R1: unmasking IRQs, waiting for ");
  serial_print_dec(SMOKE_TICKS);
  serial_puts(" ticks @ 10 Hz\n");

  uint64_t t0 = arch_timer_counter();
  irq_enable();
  while (arch_timer_ticks() < SMOKE_TICKS) cpu_wait_for_interrupt();
  uint64_t t1 = arch_timer_counter();

  arch_timer_stop();
  irq_disable();

  serial_puts("\nR1: gicv3+timer smoke PASS — ");
  serial_print_dec(arch_timer_ticks());
  serial_puts(" ticks, measured ~");
  serial_print_dec(freq * arch_timer_ticks() / (t1 - t0));
  serial_puts(" Hz\n");

  serial_puts("\nR1 slice complete (scheduler + PSCI next), parking\n");
  cpu_halt();
}
