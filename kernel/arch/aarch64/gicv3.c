/*
 * gicv3.c — GICv3 interrupt controller (aarch64)
 *
 * Bring-up order, and why: quiet the distributor, wake this core's
 * redistributor (its registers are the only per-CPU SGI/PPI config),
 * mark SGIs+PPIs Group 1 (they must arrive as IRQ, not FIQ — Group 0 is
 * the FIQ domain), then enable the distributor with affinity routing,
 * then the CPU interface. The CPU interface step has the one classic
 * GICv3 trap: ICC_PMR_EL1 resets to 0, which masks every priority in
 * the world — it must go to 0xFF or the core stays silent forever.
 *
 * QEMU virt (this DTB): GICD 0x08000000, GICR region 0x080a0000,
 * stride 0x20000. RK3568: GICD 0xfd400000, GICR 0xfd460000. Neither is
 * hard-coded — both come out of the /intc node's reg.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/gicv3.h>
#include <toyos/arch/aarch64/mmio.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/percpu.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* --- GICD registers (offsets from GICD base) --- */
#define GICD_CTLR 0x000
#define GICD_ISENABLER(n) (0x100 + 4 * (n))
#define GICD_ICENABLER(n) (0x180 + 4 * (n))

#define GICD_CTLR_RWP (1u << 31)
#define GICD_CTLR_ARE (1u << 4) /* affinity routing (ARE_NS at EL1) */
#define GICD_CTLR_ENABLE_G1A (1u << 1)
#define GICD_CTLR_ENABLE_G1 (1u << 0)

/* --- GICR registers (offsets from this core's RD_base frame) --- */
#define GICR_TYPER 0x008
#define GICR_WAKER 0x014
#define GICR_SGI_BASE 0x10000 /* SGI/PPI half of the frame */
#define GICR_IGROUPR0 (GICR_SGI_BASE + 0x080)
#define GICR_ISENABLER0 (GICR_SGI_BASE + 0x100)
#define GICR_ICENABLER0 (GICR_SGI_BASE + 0x180)

#define GICR_WAKER_SLEEP (1u << 1)
#define GICR_WAKER_CHILD_ASLEEP (1u << 2)

#define GICV3_MAX_RDIST_REGIONS 4

static uintptr_t gicd_base;
static struct {
  uintptr_t base;
  uint64_t size;
} rdist_regions[GICV3_MAX_RDIST_REGIONS];
static uint32_t rdist_region_count;
static uint64_t rdist_stride;

/* Per-CPU: this core's redistributor frame, found by affinity match — the
 * LAPIC base of the GIC world. Indexed by cpu_local slot; valid only after
 * that core ran gicv3_init_ap. */
static uintptr_t rdist_base[MAX_CPUS];

static inline uintptr_t this_rdist(void) {
  return rdist_base[this_cpu()->index];
}

static gicv3_handler_t handlers[GICV3_INTID_MAX];

static int gicd_wait_rwp(void) {
  for (int i = 0; i < 1000000; i++) {
    if ((mmio_read32(gicd_base + GICD_CTLR) & GICD_CTLR_RWP) == 0) return 0;
  }
  return -1;
}

/* reg = <GICD_base GICD_size> , <GICR_region_base GICR_region_size> ...
 * The GIC sits at the DTB root on both targets, where address- and
 * size-cells are both 2 — so tuples are 16 bytes. */
static int gicv3_parse_fdt(const void* dtb) {
  fdt_node_t node = fdt_find_compatible(dtb, "arm,gic-v3");
  if (node < 0) {
    serial_puts("gic: no arm,gic-v3 node in the dtb\n");
    return -1;
  }

  const void* reg;
  int reglen = fdt_get_prop(dtb, node, "reg", &reg);
  if (reglen < 16) {
    serial_puts("gic: /intc reg too short\n");
    return -1;
  }

  gicd_base = mmio((uintptr_t)fdt_cell64(reg)); /* PA -> live device window */

  uint32_t regions = 1;
  fdt_get_prop_u32(dtb, node, "#redistributor-regions", &regions);
  if (regions == 0 || regions > GICV3_MAX_RDIST_REGIONS) {
    serial_puts("gic: bogus #redistributor-regions\n");
    return -1;
  }
  rdist_region_count = regions;

  /* Absent stride property means the GICv3 default, 64 KiB RD + 64 KiB
   * SGI frame per core. */
  rdist_stride = 0x20000;
  const void* stride;
  int slen = fdt_get_prop(dtb, node, "redistributor-stride", &stride);
  if (slen >= 8) rdist_stride = fdt_cell64(stride);

  if ((uint32_t)reglen < 16 * (1 + regions)) {
    serial_puts("gic: reg has fewer tuples than redistributor regions\n");
    return -1;
  }
  for (uint32_t i = 0; i < regions; i++) {
    const void* tuple = (const uint8_t*)reg + 16 * (1 + i);
    rdist_regions[i].base = (uintptr_t)fdt_cell64(tuple);
    rdist_regions[i].size = fdt_cell64((const uint8_t*)tuple + 8);
  }

  serial_puts("gic: GICD @ ");
  serial_print_hex(gicd_base);
  serial_puts(", GICR @ ");
  serial_print_hex(rdist_regions[0].base);
  serial_puts(", stride ");
  serial_print_hex(rdist_stride);
  serial_puts("\n");
  return 0;
}

/* Match this core's MPIDR affinity against each frame's GICR_TYPER —
 * position in the frame array is not the CPU id. Then wake it: while
 * Sleep=1 much of the frame reads as zero, so this precedes all other
 * GICR access. */
static int gicr_find_and_wake(void) {
  uint64_t mpidr = read_mpidr();
  uint32_t want =
      (uint32_t)((mpidr & 0xff) | (mpidr & 0xff00) | (mpidr & 0xff0000) |
                 ((mpidr & 0xff00000000ull) >> 8));

  for (uint32_t r = 0; r < rdist_region_count; r++) {
    uint64_t frames = rdist_regions[r].size / rdist_stride;
    for (uint64_t i = 0; i < frames; i++) {
      uintptr_t rd = mmio(rdist_regions[r].base) + i * rdist_stride;
      uint32_t typer_hi = mmio_read32(rd + GICR_TYPER + 4);
      if (typer_hi != want) continue;

      rdist_base[this_cpu()->index] = rd;
      uint32_t waker = mmio_read32(rd + GICR_WAKER);
      if (waker & GICR_WAKER_SLEEP) {
        mmio_write32(rd + GICR_WAKER, waker & ~GICR_WAKER_SLEEP);
        int awake = 0;
        for (int spin = 0; spin < 1000000; spin++) {
          if ((mmio_read32(rd + GICR_WAKER) & GICR_WAKER_CHILD_ASLEEP) == 0) {
            awake = 1;
            break;
          }
        }
        if (!awake) {
          serial_puts("gic: redistributor never woke (ChildAsleep stuck)\n");
          return -1;
        }
      }
      return 0;
    }
  }

  serial_puts("gic: no redistributor matches affinity 0x");
  serial_print_hex(want);
  serial_puts("\n");
  return -1;
}

int gicv3_init(const void* dtb) {
  if (gicv3_parse_fdt(dtb) != 0) return -1;

  /* Quiet the distributor while we rewire it — the x86 instinct of
   * masking the 8259A before remapping. */
  mmio_write32(gicd_base + GICD_CTLR, 0);
  if (gicd_wait_rwp() != 0) {
    serial_puts("gic: GICD_CTLR RWP stuck on disable\n");
    return -1;
  }

  /* This core's redistributor + CPU interface (per-CPU registers — the
   * same routine every AP runs from secondary_main). */
  if (gicv3_init_ap() != 0) return -1;

  /* Distributor on: affinity routing first, Group 1 forwarding with it. */
  mmio_write32(gicd_base + GICD_CTLR,
               GICD_CTLR_ARE | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1);
  if (gicd_wait_rwp() != 0) {
    serial_puts("gic: GICD_CTLR RWP stuck on enable\n");
    return -1;
  }
  barrier_dsb_sy();

  serial_puts("gic: distributor + redistributor + cpu interface online\n");
  return 0;
}

/* Per-core bring-up: wake THIS core's redistributor, route SGIs+PPIs to
 * Group 1, enable the CPU interface. Run by the BSP inside gicv3_init and
 * by every AP inside secondary_main — the GICR frame, IGROUPR0, PMR and
 * IGRPEN1 are all per-core state, and a core with a sleeping GICR or a
 * zero PMR receives nothing. */
int gicv3_init_ap(void) {
  if (gicr_find_and_wake() != 0) return -1;
  uintptr_t rd = this_rdist();

  serial_printf("[gic] core %u redistributor @ %x\n",
                (uint64_t)this_cpu()->index, (uint64_t)rd);

  /* All SGIs+PPIs → Group 1 non-secure: they arrive as IRQ. Group 0 is
   * the FIQ domain and nothing in the design uses it. Priorities keep
   * their reset value 0 (highest); the PMR below lets them through. */
  mmio_write32(rd + GICR_IGROUPR0, 0xffffffff);

  /* CPU interface. SRE must latch or none of the ICC_* accesses are
   * real; PMR must be raised from its reset 0 before anything can be
   * signaled; only then does Group 1 forwarding turn on. */
  sysreg_write(ICC_SRE_EL1, 1);
  barrier_isb();
  if ((sysreg_read(ICC_SRE_EL1) & 1) == 0) {
    serial_puts("gic: ICC_SRE_EL1.SRE will not latch — no sysreg CPU iface\n");
    return -1;
  }
  sysreg_write(ICC_PMR_EL1, 0xff);
  sysreg_write(ICC_IGRPEN1_EL1, 1);
  barrier_isb();

  /* Enable the SGIs we use — LAST. Real hardware resets SGIs enabled
   * (GICR_ISENABLER0 = 0x0000FFFF), but QEMU's GICv3 model resets them
   * to 0 (hw/intc/arm_gicv3_common.c: cs->gicr_ienabler0 = 0), so this
   * write is required under QEMU or SGIs are never delivered. Placement
   * matters under QEMU 11.0.1 HVF: before the CPU-interface block, with
   * value 0xffff, this same write tripped the backend's assert(isv) on
   * the MMIO-exit path; after the block, with just the ids in use
   * (0 = resched, 1 = echo, 2 = trampoline drop), both accelerators run
   * clean. PPIs stay off until individually enabled (the timer's 27). */
  mmio_write32(rd + GICR_ISENABLER0,
               (1u << GICV3_SGI_RESCHED) | (1u << 1) | (1u << 2));

  return 0;
}

void gicv3_register_handler(uint32_t intid, gicv3_handler_t handler) {
  if (intid < GICV3_INTID_MAX) handlers[intid] = handler;
}

void gicv3_enable_intid(uint32_t intid) {
  if (intid < 32) {
    mmio_write32(this_rdist() + GICR_ISENABLER0, 1u << intid);
  } else if (intid < GICV3_INTID_MAX) {
    mmio_write32(gicd_base + GICD_ISENABLER(intid >> 5), 1u << (intid & 31));
  }
  barrier_dsb_sy(); /* posted MMIO write: drain before IRQs can rely on it */
}

void gicv3_disable_intid(uint32_t intid) {
  if (intid < 32) {
    mmio_write32(this_rdist() + GICR_ICENABLER0, 1u << intid);
  } else if (intid < GICV3_INTID_MAX) {
    mmio_write32(gicd_base + GICD_ICENABLER(intid >> 5), 1u << (intid & 31));
  }
  barrier_dsb_sy();
}

void gicv3_send_sgi(uint32_t sgi, uint32_t aff0_targets, bool all_except_self) {
  /* ICC_SGI1R_EL1 is the IPI primitive (the LAPIC ICR analogue, minus the
   * per-target command formatting — one register write). Layout:
   * TargetList [15:0] (bit per Aff0 cpu), INTID [27:24], Aff2 [39:32],
   * IRM [40] (route to all cores except self, ignoring TargetList),
   * Aff3 [55:48]. Both targets are single-cluster (Aff1..3 = 0); a
   * multi-cluster board fills in the Aff fields here. */
  uint64_t val = ((uint64_t)(sgi & 0xf) << 24);
  if (all_except_self) {
    val |= (1ull << 40);
  } else {
    val |= (aff0_targets & 0xffff);
  }
  sysreg_write(ICC_SGI1R_EL1, val);
}

void gicv3_irq_enter(void) {
  /* Mask to the INTID field [9:0]: for SGIs the ack also carries the
   * sender's CPU id in the upper bits (GICv2's IAR had it in [12:10];
   * here it sits above the INTID) — indexing the handler table with it
   * would dispatch garbage. 1020-1023 fit the same field. */
  uint32_t intid = (uint32_t)sysreg_read(ICC_IAR1_EL1) & 0x3ff;

  if (intid >= 1020) return; /* 1023 spurious (1020-1022 LPI-only): no EOI */

  gicv3_handler_t handler = handlers[intid];
  if (handler) {
    /* The handler owns its EOI: a handler may context-switch (the timer
     * tick calls schedule()), and a switch never returns through this
     * frame — a deferred EOI here would never run. Handlers that do not
     * switch simply call gicv3_eoi() themselves before returning (the
     * ToyOS64 irq.c "kernel handler owns its EOI" rule). */
    handler(intid);
  } else {
    serial_printf("\n!! irq: unhandled INTID %u — EOI'd by dispatcher\n",
                  (uint64_t)intid);
    gicv3_eoi(intid);
  }
}

void gicv3_eoi(uint32_t intid) {
  if (intid < 1020) sysreg_write(ICC_EOIR1_EL1, intid);
}
