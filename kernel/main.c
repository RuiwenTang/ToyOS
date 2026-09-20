/*
 * main.c — kernel C entry point (aarch64)
 *
 * entry.S has already settled the core to EL1, cleared .bss and set up the
 * boot stack; kmain receives the DTB pointer from the boot contract (x0).
 *
 * Boot order (R1 final shape, mirroring ToyOS64's kernel_main for
 * everything this slice ports):
 *   percpu probe → MMU → FDT → GIC → timer → percpu → heap → kstack →
 *   sleep → sched_init (+ smoke workers) → timer arm → PSCI bring-up →
 *   SGI echo → per-core workers → sched_start
 * All with IRQs masked until sched_start (except the bounded echo-test
 * window, safe because pre-start ticks are gated to EOI+reload). From
 * sched_start on, kmain's context IS the boot/idle thread.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/smp.h>
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

/* --- Tick-preempt smoke, BSP (R1 acceptance: "preempt ticks") ---
 *
 * Each worker busy-waits through >= 2 ticks per iteration and never yields
 * — on this single core the ONLY way all SMOKE_THREADS workers can finish
 * is the timer tick preempting between them, which is exactly what the
 * smoke asserts (interleaved "worker N iter I" lines on serial). The last
 * one out prints the PASS line and everyone thread_exit()s, which also
 * exercises the reap path (switch_prev → thread_release). */
static volatile uint32_t smoke_done;

static void smoke_worker(void* arg) {
  uintptr_t id = (uintptr_t)arg;

  for (int i = 1; i <= SMOKE_ITERS; i++) {
    uint64_t start = timer_get_ticks();
    while (timer_get_ticks() - start <
           2); /* spin: no yield — the tick must move us off */

    serial_printf("sched: worker %u iter %u (tick-preempted)\n", (uint64_t)id,
                  (uint64_t)i);
  }

  uint32_t done = atomic_add_return(&smoke_done, 1);
  if (done == SMOKE_THREADS) {
    serial_printf(
        "R1: scheduler smoke PASS — %u ticks, %u workers round-robin\n",
        timer_get_ticks(), (uint64_t)SMOKE_THREADS);
  }

  thread_exit();
}

/* --- Per-core preempt smoke, APs (R1 acceptance: "all cores online") ---
 *
 * Two workers per AP, each spinning ~4 ticks per iteration with no yield:
 * they can only interleave if THAT core's own tick preempts between them —
 * the BSP proof, replicated per core. The spins use the local counter
 * (arch_timer_counter), not BSP-owned system_ticks, so an AP worker is
 * self-contained: its clock and its preemption both come from its own
 * CNTV tick. */
#define SMP_WORKERS_PER_CORE 2
#define SMP_SMOKE_ITERS 3

static volatile uint32_t smp_smoke_done;

static void smp_worker(void* arg) {
  uintptr_t id = (uintptr_t)arg; /* (cpu << 8) | worker */
  uint32_t cpu = (uint32_t)(id >> 8);
  uint32_t w = (uint32_t)(id & 0xff);
  uint64_t spin = arch_timer_freq() / 250; /* 4 ms = 4 ticks */

  for (int i = 1; i <= SMP_SMOKE_ITERS; i++) {
    uint64_t start = arch_timer_counter();
    while (arch_timer_counter() - start < spin);

    serial_printf("smp: cpu %u w%u iter %u (tick-preempted)\n", (uint64_t)cpu,
                  (uint64_t)w, (uint64_t)i);
  }

  uint32_t done = atomic_add_return(&smp_smoke_done, 1);
  uint32_t total = (ncpus - 1) * SMP_WORKERS_PER_CORE;
  if (done == total) {
    serial_printf(
        "SMP: per-core preempt smoke PASS — %u workers across %u cores\n",
        (uint64_t)total, (uint64_t)(ncpus - 1));
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
  smp_probe(dtb); /* /cpus slot table + PSCI conduit (prints its findings) */
  percpu_init();  /* all slots; TPIDR_EL1 → slot 0 (GICR lookup needs it) */
  if (gicv3_init(dtb) != 0) {
    serial_puts("gic: init failed, halting\n");
    cpu_halt();
  }
  arch_timer_init();

  serial_puts("\nR1: scheduler bring-up\n");
  kernel_heap_init();
  kstack_init();
  sleep_init();
  sched_init(); /* creates every AP's idle thread (ncpus slots) */

  for (uintptr_t i = 0; i < SMOKE_THREADS; i++)
    thread_create("smoke", smoke_worker, (void*)i);

  timer_init(); /* registers + arms the BSP tick; IRQs stay masked */

  smp_ipi_init();          /* SGI handlers, before any core can receive one */
  smp_start_secondaries(); /* PSCI CPU_ON + online handshake */
  smp_echo_test();         /* R1 acceptance: IPI echo */

  for (unsigned c = 1; c < ncpus; c++)
    for (unsigned w = 0; w < SMP_WORKERS_PER_CORE; w++)
      thread_create_on(c, "smpw", smp_worker, (void*)(uintptr_t)((c << 8) | w));

  /* Unmasks IRQs, swaps onto the boot kstack, first schedule() — never
   * returns. The workers above run to completion (tick preempted), then
   * this context (the boot/idle thread) parks in WFI. */
  sched_start();
}
