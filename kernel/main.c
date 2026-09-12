/*
 * main.c — kernel C entry point (aarch64)
 *
 * entry.S has already settled the core to EL1, cleared .bss and set up the
 * boot stack; kmain receives the DTB pointer from the boot contract (x0).
 *
 * R0 scope: console up, banner with the boot state, FDT walk, park. The
 * SMP/exception/scheduler phases follow the roadmap in
 * docs/aarch64-port-arch.md (R1+).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

void kmain(const void* dtb) {
  uint32_t size;

  serial_init();
  serial_puts("\nToyOS aarch64 R0\n");
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
  serial_puts("\nR0 complete, parking\n");
  cpu_halt();
}
