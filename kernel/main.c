/*
 * main.c — kernel C entry point (aarch64)
 *
 * entry.S has already settled the core to EL1, cleared .bss and set up the
 * boot stack; kmain_low receives the DTB pointer from the boot contract
 * (x0, a physical address).
 *
 * Boot order (R2.2 shape):
 *   kmain_low (executing at the physical load base, identity trampoline):
 *     serial → FDT + memmap discovery → bootmmu (BOTH trees) → jump to
 *     the link VA (bootmmu_to_high)
 *   kmain_high (the link VA, kernel tree):
 *     DTB dump → SVC smoke → SMP probe → percpu → GIC → timer → pmm (+
 *     smoke) → heap (pmm provider) → kstack (real, guarded) → sleep →
 *     sched_init (+ smoke workers) → timer arm → PSCI bring-up → SGI echo
 *     → device-window flip + per-core trampoline drop → guard-overflow
 *     smoke threads → sched_start
 * All with IRQs masked until sched_start (except the bounded echo/drop
 * windows, safe because pre-start ticks are gated to EOI+reload). From
 * sched_start on, kmain_high's context IS the boot/idle thread.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/kva.h>
#include <toyos/arch/aarch64/smp.h>
#include <toyos/arch/aarch64/trap.h>
#include <toyos/kernel/atomic.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/heap.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/memmap.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/pmm.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/timer.h>
#include <toyos/kernel/types.h>

/* Whole-image extent from the linker (link VAs; header slot through the
 * boot stack, .bss and the AP boot stacks live inside). */
extern const char __kernel_start[], __kernel_end[];

void kmain_high(uintptr_t dtb_pa); /* kmain_low's high-VA continuation */

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

/* --- R2.2 acceptance: guard-page overflow faults and kills ---
 *
 * The overflow worker walks a volatile write pointer down from its own
 * frame, half-page by half-page, until it steps into its slot's guard
 * page — the unmapped L3 entry turns the store into a data abort, traps.c
 * recognises the kstack-region FAR, and thread_exit() kills it. The
 * witness (a separate, healthy thread) polls the kill counter and prints
 * the PASS line, proving the kernel survived the overflow intact. */
static void overflow_worker(void* arg) {
  (void)arg;
  volatile char probe = 0;
  (void)probe;
  char* p = (char*)((uintptr_t)&probe & ~(uintptr_t)(PAGE_SIZE - 1));

  for (;;) {
    p -= 512;
    *p = 1; /* until the guard page faults */
  }
}

static void overflow_witness(void* arg) {
  (void)arg;
  uint64_t deadline = timer_get_ticks() + 200; /* ~2 s at 100 Hz */

  while (kstack_guard_hits == 0) {
    if (timer_get_ticks() > deadline) {
      serial_puts("R2.2: kstack guard smoke FAIL — overflow never trapped\n");
      thread_exit();
    }
  }

  serial_puts(
      "R2.2: kstack guard smoke PASS — overflow faulted at the guard and "
      "the thread was killed, kernel intact\n");
  thread_exit();
}

/* --- R2.1 pmm smoke (heap-on-pmm preconditions) ---
 *
 * Accounting + contiguity + a write through the direct map: alloc/free
 * must move the free count by exactly the right amounts and come back to
 * the baseline, the 4-page alloc must be physically contiguous, and both
 * ends must be writable through pmm_phys_to_virt (a Normal-WB-typed page
 * per the kernel tree — a mis-typed or unmapped page faults right here
 * instead of inside the scheduler later). Runs before kernel_heap_init so
 * the heap's own pages are never interleaved with the smoke's. */
static int pmm_smoke(void) {
  size_t free0 = pmm_free_page_count();

  uintptr_t p1 = pmm_alloc();
  if (p1 == 0 || (p1 & (PAGE_SIZE - 1)) != 0) return 0;
  if (pmm_free_page_count() != free0 - 1) return 0;

  uintptr_t p4 = pmm_alloc_pages(4);
  if (p4 == 0 || (p4 & (PAGE_SIZE - 1)) != 0) return 0;
  if (pmm_free_page_count() != free0 - 5) return 0;
  /* the two allocations must not overlap */
  if (p1 >= p4 && p1 < p4 + 4 * PAGE_SIZE) return 0;
  for (int i = 0; i < 4; i += 3) { /* first + last page writable via DM */
    volatile uint64_t* page = pmm_phys_to_virt(p4 + (uintptr_t)i * PAGE_SIZE);
    *page = 0x524F5953ull; /* "SYOR" */
    if (*page != 0x524F5953ull) return 0;
  }

  /* refcounts: fresh frame is 1, inc/dec balanced */
  if (pmm_refcount_get(p1) != 1) return 0;
  pmm_refcount_inc(p1);
  if (pmm_refcount_get(p1) != 2) return 0;
  if (pmm_refcount_dec(p1) != 1) return 0;

  pmm_free(p1);
  pmm_free_pages(p4, 4);
  return pmm_free_page_count() == free0;
}

/*
 * kmain_low — everything that runs at the physical load base.
 *
 * The pre-MMU subset is exactly what stays legal on all-Device memory:
 * plain (narrow, volatile where it matters) loads and single-core unlocked
 * serial. Memory discovery must precede bootmmu so the tables map real
 * banks; the DTB pointer and the linker-extent conversions all use the
 * physical world's arithmetic (kimage_offset for image VAs).
 */
void kmain_low(const void* dtb) {
  uint32_t size;
  /* Image PA base: KERNEL_IMAGE_BASE (constant link VA) minus the offset
   * entry.S published. NOT kva_to_pa(&symbol) — see the kva.h warning. */
  uint64_t image_pa = KERNEL_IMAGE_BASE - kimage_offset;

  serial_init();
  serial_puts("\nToyOS aarch64 R2 (higher-half)\n");
  serial_puts("boot EL: EL");
  serial_print_dec(current_el());
  serial_puts(", dtb @ ");
  serial_print_hex((uintptr_t)dtb);
  serial_puts(", image at ");
  serial_print_hex(image_pa);
  serial_puts("\n");

  size = fdt_valid(dtb);
  if (size == 0) {
    serial_puts("fdt: invalid blob, halting\n");
    cpu_halt();
  }
  serial_puts("fdt: blob size ");
  serial_print_dec(size);
  serial_puts("\n");

  /* Image PA extent. KERNEL_IMAGE_BASE is an absolute constant (its link
   * VA), and the symbol difference is position-independent — C symbol
   * references here resolve to run addresses (we are low), so kva_to_pa
   * must NOT be applied to them (double-counting; the kva.h warning). */
  {
    uint64_t image_size =
        (uint64_t)(uintptr_t)__kernel_end - (uint64_t)(uintptr_t)__kernel_start;
    memmap_init(dtb, image_pa, image_pa + image_size,
                (uint64_t)(uintptr_t)dtb + size);
  }

  {
    size_t nbanks = 0;
    const mem_region_t* banks = memmap_banks(&nbanks);
    bootmmu_init(banks, nbanks);
  }

  /* Both trees live: transfer to the link VA. The boot stack and VBAR
   * move with us (same memory, new addresses); execution never returns
   * below the fold. */
  bootmmu_to_high((uintptr_t)dtb);
}

/*
 * kmain_high — everything after the switchover. @dtb_pa is the physical
 * handoff pointer; RAM objects are reached through the direct map.
 */
void kmain_high(uintptr_t dtb_pa) {
  const void* dtb = (const void*)kva_dm(dtb_pa);

  serial_puts("R2.2: executing at the link VA — higher-half switch OK\n");

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

  serial_puts("\nR2.1: physical memory manager\n");
  {
    size_t nregions = 0;
    const mem_region_t* usable = memmap_usable(&nregions);
    pmm_init(usable, nregions);
  }
  if (pmm_smoke()) {
    serial_puts(
        "R2.1: pmm smoke PASS — alloc/free accounting + contiguity "
        "+ direct-map writeback\n");
  } else {
    serial_puts("R2.1: pmm smoke FAIL, halting\n");
    cpu_halt();
  }

  serial_puts("\nR1: scheduler bring-up\n");
  kernel_heap_init();
  kstack_init(); /* real guarded allocator (dedicated VA region) */
  sleep_init();
  sched_init(); /* creates every AP's idle thread (ncpus slots) */

  for (uintptr_t i = 0; i < SMOKE_THREADS; i++)
    thread_create("smoke", smoke_worker, (void*)i);

  timer_init(); /* registers + arms the BSP tick; IRQs stay masked */

  smp_ipi_init();          /* SGI handlers, before any core can receive one */
  smp_start_secondaries(); /* PSCI CPU_ON + online handshake */
  smp_echo_test();         /* R1 acceptance: IPI echo */

  /* R2.2 acceptance: device window to the direct map, then the identity
   * trampoline away on every core — from here nothing below the fold
   * exists anywhere. */
  smp_drop_trampolines();

  for (unsigned c = 1; c < ncpus; c++)
    for (unsigned w = 0; w < SMP_WORKERS_PER_CORE; w++)
      thread_create_on(c, "smpw", smp_worker, (void*)(uintptr_t)((c << 8) | w));

  /* Guard-overflow proof (after the worker smoke so the R1 lines land
   * first; serial order between them is not asserted anyway). */
  thread_create("ovfw", overflow_worker, NULL);
  thread_create("ovwt", overflow_witness, NULL);

  /* Unmasks IRQs, swaps onto the boot kstack, first schedule() — never
   * returns. The workers above run to completion (tick preempted), then
   * this context (the boot/idle thread) parks in WFI. */
  sched_start();
}
