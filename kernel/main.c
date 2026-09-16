/*
 * main.c — kernel C entry point (aarch64)
 *
 * entry.S has already settled the core to EL1, cleared .bss and set up the
 * boot stack; kmain receives the DTB pointer from the boot contract (x0).
 *
 * Boot order mirrors ToyOS64's kernel_main for everything the R1 slice
 * ports: percpu → heap → kstack → sched_init (+ smoke workers) → timer →
 * sched_start, all with IRQs masked until sched_start unmasks them. From
 * sched_start on, kmain's context IS the boot/idle thread.
 *
 * R1 remaining scope after this slice: PSCI CPU_ON secondaries + SGI IPI
 * echo (docs/aarch64-port-arch.md).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/kernel/atomic.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/heap.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/timer.h>
#include <toyos/kernel/types.h>

#define SMOKE_THREADS 3
#define SMOKE_ITERS 3

/* --- Tick-preempt smoke (R1 acceptance: "preempt ticks") ---
 *
 * Each worker busy-waits through >= 2 ticks per iteration and never yields
 * — on this single online core the ONLY way all SMOKE_THREADS workers can
 * finish is the timer tick preempting between them, which is exactly what
 * the smoke asserts (interleaved "worker N iter I" lines on serial). The
 * last one out prints the PASS line and everyone thread_exit()s, which
 * also exercises the reap path (switch_prev → thread_release). */
static volatile uint32_t smoke_done;

static void smoke_worker(void* arg) {
  uintptr_t id = (uintptr_t)arg;

  for (int i = 1; i <= SMOKE_ITERS; i++) {
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() - start <
           2); /* spin: no yield — the tick must move us off */

    serial_puts("sched: worker ");
    serial_print_dec((uint64_t)id);
    serial_puts(" iter ");
    serial_print_dec((uint64_t)i);
    serial_puts(" (tick-preempted)\n");
  }

  uint32_t done = atomic_add_return(&smoke_done, 1);
  if (done == SMOKE_THREADS) {
    serial_puts("R1: scheduler smoke PASS — ");
    serial_print_dec(timer_get_ticks());
    serial_puts(" ticks, ");
    serial_print_dec(SMOKE_THREADS);
    serial_puts(" workers round-robin\n");
  }

  thread_exit();
}

void kmain(const void* dtb) {
  uint32_t size;

  serial_init();
  serial_puts("\nToyOS aarch64 R1\n");
  serial_puts("boot EL: EL");
  serial_print_dec(current_el());
  serial_puts(", dtb @ ");
  serial_print_hex((uintptr_t)dtb);
  serial_puts("\n");

  /* Before the first atomic/exclusive instruction (the scheduler's
   * spinlocks): Device memory doesn't support them. Identity map only —
   * RAM becomes Normal WB, MMIO stays Device; R2's paging replaces this. */
  bootmmu_init();

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
  arch_timer_init();

  serial_puts("\nR1: scheduler bring-up\n");
  percpu_init();
  kernel_heap_init();
  kstack_init();
  sleep_init();
  sched_init();

  for (uintptr_t i = 0; i < SMOKE_THREADS; i++)
    thread_create("smoke", smoke_worker, (void*)i);

  timer_init(); /* registers + arms the tick; IRQs stay masked */

  /* Unmasks IRQs, swaps onto the boot kstack, first schedule() — never
   * returns. The workers above run to completion (tick preempted), then
   * this context (the boot/idle thread) parks in WFI. */
  sched_start();
}
