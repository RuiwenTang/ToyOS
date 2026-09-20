/*
 * gicv3.h — GICv3 interrupt controller driver (aarch64)
 *
 * The x86 analogue is split in two here: GICD ≈ IOAPIC (global SPI
 * routing), GICR ≈ the per-CPU LAPIC half, and the CPU interface is
 * system registers instead of MMIO — reading ICC_IAR1_EL1 is the ack
 * (x86 pushes the vector for you), writing ICC_EOIR1_EL1 is the EOI.
 *
 * R1 scope: BSP bring-up, SPI/PPI/SGI only (no ITS/LPI — the blueprint
 * rules MSIs out). Bases and stride come from the DTB, so the RK3568
 * needs no #ifdefs. The handler table here is the seam the copied
 * kernel/intr/irq.c takes over when the scheduler port lands.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_GICV3_H
#define TOYOS_ARCH_AARCH64_GICV3_H

#include <stdbool.h>
#include <toyos/kernel/types.h>

/* INTID space: 0-15 SGI, 16-31 PPI, 32-1019 SPI, 1020-1023 special. */
#define GICV3_INTID_MAX 1020
#define GICV3_INTID_SPURIOUS 1023

/* SGI assignments (software convention — hardware treats 0-15 alike).
 * RESCHED wakes an idle core (WFI retires, sched_idle_loop re-checks its
 * run queue); the R1 echo smoke uses its own id in smp.c. */
#define GICV3_SGI_RESCHED 0

typedef void (*gicv3_handler_t)(uint32_t intid);

/* Bring the GIC up on the BSP: discover bases from the DTB, then the
 * per-core sequence below plus the distributor (affinity routing + Group 1
 * forwarding). Returns 0, or -1 after a loud failure message. */
int gicv3_init(const void* dtb);

/* Per-core bring-up: wake THIS core's redistributor, route SGIs+PPIs to
 * Group 1, program this core's CPU interface (SRE/PMR/IGRPEN1). Run by
 * every AP from secondary_main; gicv3_init runs it for the BSP. Requires
 * TPIDR_EL1 to be bound (the rdist slot is indexed by cpu_local). */
int gicv3_init_ap(void);

/* Bind a handler to an INTID, and enable/disable its forwarding. For
 * INTIDs < 32 (SGI/PPI) enable/disable hits the CALLING core's GICR —
 * arming the timer on an AP is that core's own business. */
void gicv3_register_handler(uint32_t intid, gicv3_handler_t handler);
void gicv3_enable_intid(uint32_t intid);
void gicv3_disable_intid(uint32_t intid);

/* Raise an SGI: @aff0_targets is a bit-per-cpu mask within Aff1..3 = 0
 * (both targets are single-cluster), or set @all_except_self for a
 * broadcast (IRM — hardware routing, TargetList ignored). One
 * ICC_SGI1R_EL1 write, no per-target programming (the LAPIC-ICR
 * analogue, simpler). */
void gicv3_send_sgi(uint32_t sgi, uint32_t aff0_targets, bool all_except_self);

/* IRQ entry from the vector dispatcher: ack (ICC_IAR1_EL1) and run the
 * bound handler. A registered handler OWNS its EOI — it may context-switch
 * (the timer tick) and never return through this frame, so it must call
 * gicv3_eoi() itself first (re-arm → EOI → schedule is the timer's order).
 * Unhandled INTIDs are EOI'd here. Spurious acks (INTID 1023) return
 * without an EOI, per the architecture. */
void gicv3_irq_enter(void);

/* End of interrupt (priority drop + deactivate, ICC_EOIR1_EL1). Safe no-op
 * for the special INTIDs (>= 1020). */
void gicv3_eoi(uint32_t intid);

#endif /* TOYOS_ARCH_AARCH64_GICV3_H */
