/*
 * smp.c — secondary-core bring-up + inter-processor interrupts (aarch64)
 *
 * Bring-up contract (the part every aarch64 SMP kernel must get right, in
 * this order, per core):
 *   1. MMU + caches ON before the first atomic/exclusive — a core released
 *      by CPU_ON runs with the MMU off, and all-Device memory faults
 *      LDAXR/STXR (the heap_lock DFSC 0x35 lesson). secondary_entry calls
 *      bootmmu_ap_enable (same shared identity tables) as its first C call.
 *   2. percpu bind (TPIDR_EL1) — this_cpu() is garbage until then.
 *   3. per-core GICR wake + CPU interface, per-core CNTV arming.
 *   4. online handshake (release store here, acquire load on the BSP).
 *
 * The BSP drives 1-4 indirectly: PSCI CPU_ON per /cpus entry, then a
 * bounded spin on the online mask. Slot 0 is always the BSP (the first
 * /cpus node is the boot cpu by DT convention; the probe enforces it).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/arch_timer.h>
#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/psci.h>
#include <toyos/arch/aarch64/smp.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/atomic.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/kstack.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/sched.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/string.h>
#include <toyos/kernel/timer.h>
#include <toyos/kernel/types.h>

/* Aff0-2 in bits [23:0], Aff3 in bits [39:32] — everything else in MPIDR
 * (MT bit, the RES1 bit 31) is not affinity. */
#define MPIDR_AFF_MASK (0xffffffull | (0xffull << 32))

#define SGI_ECHO 1 /* sched owns SGI 0 (GICV3_SGI_RESCHED) */

/* 16 KiB per AP, matching the entry.S reservation (slot i-1 for core i). */
#define AP_BOOT_STACK_SIZE 0x4000
#define MAX_APS (MAX_CPUS - 1)

/* Set by secondary_entry (entry.S) — CPU_ON's entry point argument. */
extern void secondary_entry(void);

static struct {
  uint64_t mpidr[MAX_CPUS];
  unsigned count;
  int psci_ok;
} smp;

static volatile uint32_t smp_online_mask; /* bit i set once core i is up */

unsigned smp_cpu_count(void) { return smp.count; }

uint64_t smp_cpu_mpidr(unsigned i) { return i < smp.count ? smp.mpidr[i] : 0; }

void smp_probe(const void* dtb) {
  uint64_t boot_aff = read_mpidr() & MPIDR_AFF_MASK;

  smp.count = 1;
  smp.mpidr[0] = boot_aff;
  smp.psci_ok = psci_init(dtb) == 0;

  fdt_node_t cpus = fdt_find_node(dtb, "/cpus");
  if (cpus < 0) {
    serial_puts("smp: no /cpus node — single core\n");
    goto out;
  }

  unsigned n = 0;
  for (fdt_node_t c = fdt_child_first(dtb, cpus); c >= 0;
       c = fdt_child_next(dtb, c)) {
    const void* type;
    if (fdt_get_prop(dtb, c, "device_type", &type) < 4) continue;
    if (strcmp((const char*)type, "cpu") != 0) continue;

    const void* reg;
    int len = fdt_get_prop(dtb, c, "reg", &reg);
    if (len < 4) continue;
    if (n < MAX_CPUS)
      smp.mpidr[n++] = (len >= 8) ? fdt_cell64(reg) : fdt_cell32(reg);
  }
  if (n == 0) {
    serial_puts("smp: /cpus has no cpu children — single core\n");
    goto out;
  }

  /* Slot 0 must be the running core: every affinity-keyed lookup (GICR
   * TYPER match, CPU_ON target) uses slot order, and the DT convention is
   * boot-cpu-first — enforce rather than assume. */
  if ((smp.mpidr[0] & MPIDR_AFF_MASK) != boot_aff) {
    for (unsigned j = 1; j < n; j++) {
      if ((smp.mpidr[j] & MPIDR_AFF_MASK) == boot_aff) {
        uint64_t t = smp.mpidr[0];
        smp.mpidr[0] = smp.mpidr[j];
        smp.mpidr[j] = t;
        break;
      }
    }
  }
  smp.count = n;

out:
  if (!smp.psci_ok && smp.count > 1) {
    serial_puts("smp: PSCI unavailable — degrading to 1 core\n");
    smp.count = 1;
  }
  serial_printf("smp: %u cpu(s) (boot mpidr %x)\n", (uint64_t)smp.count,
                boot_aff);
}

/* --- SGI handlers --- */

static volatile uint32_t echo_rx;              /* BSP-side: echoes received */
static volatile uint32_t echo_count[MAX_CPUS]; /* per-AP: echoes sent back */

/* Wake-only: the target's sched_idle_loop re-checks its run queue after
 * WFI, which is the whole point — the kick, not the handler body. */
static void sgi_resched_handler(uint32_t intid) { gicv3_eoi(intid); }

static void sgi_echo_handler(uint32_t intid) {
  gicv3_eoi(intid);
  uint32_t me = this_cpu()->index;

  if (me == 0) {
    echo_rx++; /* single writer (BSP IRQ context) */
    return;
  }
  echo_count[me]++;
  serial_printf("ipi: core %u echo #%u\n", (uint64_t)me,
                (uint64_t)echo_count[me]);
  gicv3_send_sgi(SGI_ECHO, 1u << 0, false);
}

void smp_ipi_init(void) {
  gicv3_register_handler(GICV3_SGI_RESCHED, sgi_resched_handler);
  gicv3_register_handler(SGI_ECHO, sgi_echo_handler);
}

/* --- bring-up --- */

void smp_start_secondaries(void) {
  serial_puts("\nSMP: PSCI secondary bring-up\n");
  if (smp.count <= 1) {
    serial_puts("SMP: single core, nothing to bring up\n");
    return;
  }

  unsigned online = 1; /* the BSP */
  for (unsigned i = 1; i < smp.count; i++) {
    int32_t rc = psci_cpu_on(smp.mpidr[i], (uintptr_t)secondary_entry, i);
    if (rc != PSCI_SUCCESS) {
      serial_printf("psci: CPU_ON core %u failed (%d)\n", (uint64_t)i,
                    (int64_t)rc);
      continue;
    }

    /* Bounded wait for the online handshake: 2 s of counter. The AP is
     * fully scheduled (idle loop entered) by the time it sets its bit. */
    uint64_t deadline = arch_timer_counter() + 2 * arch_timer_freq();
    for (;;) {
      if (__atomic_load_n(&smp_online_mask, __ATOMIC_ACQUIRE) & (1u << i))
        break;
      if (arch_timer_counter() > deadline) {
        serial_printf("SMP: core %u never came online\n", (uint64_t)i);
        rc = PSCI_INTERNAL_FAILURE;
        break;
      }
    }
    if (rc == PSCI_SUCCESS) online++;
  }

  if (online == smp.count) {
    serial_printf("SMP: all %u cores online\n", (uint64_t)smp.count);
  } else {
    serial_printf("SMP: degraded — %u/%u cores online\n", (uint64_t)online,
                  (uint64_t)smp.count);
  }
}

void secondary_main(uint32_t me) {
  percpu_init_ap(me); /* TPIDR_EL1 → cpu_locals[me] (slot pre-initialised) */

  if (gicv3_init_ap() != 0) { /* this core's GICR + CPU interface */
    serial_puts("[SMP] core ");
    serial_print_dec(me);
    serial_puts(" GICR init failed — parking\n");
    cpu_halt();
  }

  /* Per-core tick: the handler is registered globally (BSP's timer_init),
   * but enabling PPI 27 and arming CNTV are per-core registers. */
  gicv3_enable_intid(ARCH_TIMER_VIRT_INTID);
  arch_timer_start(TIMER_HZ);

  /* This core's idle thread (created by sched_init on the BSP). It wraps
   * this very context the way the boot thread wraps kmain on the BSP. */
  struct thread* idle = cpu_locals[me].idle;
  if (!idle) {
    serial_puts("[SMP] core ");
    serial_print_dec(me);
    serial_puts(" has no idle thread — parking\n");
    cpu_halt();
  }
  this_cpu()->current = idle;
  idle->on_cpu = (int)me;

  /* Unmask IRQs (WFI must wake) — pre-sched_start ticks are gated to
   * EOI+reload by g_sched_started, so this window is safe — then publish
   * readiness: release-store pairs with the BSP bring-up's acquire-load. */
  irq_enable();
  __atomic_or_fetch(&smp_online_mask, 1u << me, __ATOMIC_RELEASE);

  serial_printf("[SMP] core %u online\n", (uint64_t)me);

  /* Mirror sched_start's tail: abandon the AP boot stack for the idle
   * thread's guarded kstack, first schedule(), then the idle loop. The
   * idle thread's pre-built initial frame is simply never consumed — the
   * first switch OUT of here writes a real resume SP over it. */
  uint64_t new_sp =
      (uint64_t)(uintptr_t)idle->stack_base +
      (KSTACK_GUARD_PAGES + THREAD_STACK_SIZE / PAGE_SIZE) * PAGE_SIZE;
  __asm__ volatile("mov sp, %0" ::"r"(new_sp) : "memory");

  schedule();
  sched_idle_loop(); /* noreturn */
}

/* --- R1 acceptance: IPI echo --- */

void smp_echo_test(void) {
  if (smp.count < 2) return;

  serial_puts("\nSMP: SGI echo test\n");
  irq_enable(); /* kmain context; pre-start ticks stay EOI+reload only */

  const uint32_t rounds = 3;
  for (uint32_t r = 1; r <= rounds; r++) {
    echo_rx = 0;
    gicv3_send_sgi(SGI_ECHO, 0, true); /* IRM: all cores except self */

    uint64_t deadline = arch_timer_counter() + arch_timer_freq();
    while (__atomic_load_n(&echo_rx, __ATOMIC_ACQUIRE) < smp.count - 1) {
      if (arch_timer_counter() > deadline) {
        serial_printf("SMP: IPI echo FAIL (timeout in round %u)\n",
                      (uint64_t)r);
        irq_disable();
        return;
      }
    }
    serial_printf("ipi: round %u — %u echoes\n", (uint64_t)r,
                  (uint64_t)(smp.count - 1));
  }

  irq_disable();
  serial_puts("SMP: IPI echo PASS (3 rounds x all cores)\n");
}
