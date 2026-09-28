/*
 * entry.S — ToyOS aarch64 kernel entry point
 *
 * The only boot contract (see docs/aarch64-port-arch.md "Handoff protocol"):
 *   x0 = DTB pointer, x1..x3 = 0, MMU off, caches off.
 * U-Boot's booti and QEMU's -kernel mini-loader both honour it, and both
 * may enter at EL2 (QEMU's mini-loader always does; U-Boot passes through
 * whatever it runs at). The kernel settles to EL1 itself — the x86 analogue
 * is Limine handing over long mode: firmware state is not trusted, only the
 * register contract is.
 *
 * R2.2 higher-half: the image LINKS at 0xFFFFFFFF80000000 but LOADS (and
 * first runs) at RAM base + 0x80000. Until bootmmu turns on both TTBRs,
 * execution is at the physical base and every absolute literal (=symbol)
 * holds a high VA that is unmapped — so pre-MMU code computes the
 * load/link delta once (kimage_offset) and corrects each absolute by it.
 * ADRP/adr/BL are PC-relative and land on the running base, which the
 * identity trampoline covers — C code needs no such care at either base.
 * The jump to the link VA is a single absolute transfer (bootmmu_to_high /
 * the tail of secondary_entry).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

.section .text.boot

.global _start
.type _start, @function
_start:
    /* EL2 → EL1 settle (skip if already at EL1). */
    mrs x9, CurrentEL
    lsr x9, x9, #2
    cmp x9, #2
    b.lt 1f                  /* EL1: nothing to do */
    b.gt 2f                  /* EL3: unsupported, park */

    msr hcr_el2, xzr
    mov x10, #(1 << 31)      /* HCR_EL2.RW = 1: EL1 runs AArch64 */
    msr hcr_el2, x10
    msr cptr_el2, xzr        /* no FP/SIMD traps to EL2 */
    msr sctlr_el1, xzr       /* EL1: MMU/caches/alignment checks off */
    mov x10, #0x3c5          /* SPSR_EL2: EL1h, DAIF masked */
    msr spsr_el2, x10
    adr x10, 1f
    msr elr_el2, x10
    eret                     /* x0 (DTB) survives */

1:  /* EL1, still executing at the physical load base. */
    /* Load/link delta: kimage_offset = link(symbol) - run(symbol). All
     * absolute literals below are corrected by it while running low. */
    adr x10, 1b
    ldr x11, =1b
    sub x12, x11, x10
    /* Publish it (the .data symbol's run address = link - delta). */
    ldr x13, =kimage_offset
    sub x13, x13, x12
    str x12, [x13]

    mov x19, x0              /* preserve the DTB pointer (physical) */

    ldr x0, =__stack_top
    sub x0, x0, x12          /* boot stack at its physical address */
    mov sp, x0

    /* VBAR_EL1 → the real 16-slot table in traps.S, at its physical
     * address for now (re-armed at the link VA by bootmmu_to_high). Must
     * be armed before anything can trap (bss clearing, C code). The
     * literal-pool form works at any link distance; .vectors' 2 KiB
     * alignment is the linker script's job. */
    ldr x0, =vectors
    sub x0, x0, x12
    msr vbar_el1, x0

    /* Zero .bss — RAM contents are whatever the loader left behind. */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x0, x0, x12
    sub x1, x1, x12
3:  cmp x0, x1
    b.hs 4f
    str xzr, [x0], #8
    b 3b

4:  mov x0, x19
    bl kmain_low             /* low half of boot; jumps high itself */

2:  /* kmain returned (it must not) or unsupported EL: park. */
    msr daifset, #0xf
5:  wfe
    b 5b

.size _start, . - _start

/*
 * bootmmu_to_high — absolute transfer to the link VA (R2.2 switchover).
 *
 * C prototype: void bootmmu_to_high(uintptr_t arg)  [arg reaches
 * kmain_high as its C-ABI first argument, x0]
 *
 * Both trees are live (bootmmu_init ran), so high addresses are valid
 * while PC/SP are still physical: re-arm VBAR at the link VA, move SP to
 * the same stack at its link VA, then br to kmain_high via an ABSOLUTE
 * literal. The absolute form is load-bearing: a function pointer passed
 * from C compiles to adr/adrp (position-relative), which lands on the
 * low copy of the target and the kernel would keep running at the
 * physical base. The overlap window makes every step safe in both
 * directions — the aarch64 version of the x86 "jump to the higher-half
 * label, then reload GDT/IDT/stack" dance.
 */
.section .text
.global bootmmu_to_high
.type bootmmu_to_high, @function
bootmmu_to_high:
    adr x3, bootmmu_to_high
    ldr x4, =bootmmu_to_high
    sub x4, x4, x3           /* delta = link - run */

    ldr x5, =vectors
    msr vbar_el1, x5         /* vectors at the link VA */

    mov x6, sp
    add x6, x6, x4
    mov sp, x6               /* same boot stack at its link VA */

    dsb nsh
    isb
    ldr x30, =kmain_high     /* absolute: the LINK VA, not the running copy */
    br x30                   /* x0 (arg) already in C-ABI position */
.size bootmmu_to_high, . - bootmmu_to_high

/*
 * secondary_entry — PSCI CPU_ON target for cores 1..N-1.
 *
 * Entry state (PSCI contract): x0 = context_id (this core's cpu_locals
 * slot), MMU off, caches off, IRQs masked, and — on QEMU — EL2, exactly
 * the boot contract _start sees; the settle sequence below is the same
 * one verbatim. smp.c passes this symbol's PHYSICAL address to CPU_ON.
 *
 * Order (position-safety is the R2.2 twist):
 *   settle → delta → AP boot stack (physical) → bootmmu_ap_enable (both
 *   TTBRs, runs low under the trampoline — precedes any atomic) → VBAR/
 *   SP at link VAs → absolute br secondary_main (high, never returns)
 */
.section .text
.global secondary_entry
.type secondary_entry, @function
secondary_entry:
    mrs x9, CurrentEL
    lsr x9, x9, #2
    cmp x9, #2
    b.lt 11f                 /* EL1: nothing to do */
    b.gt 12f                 /* EL3: unsupported, park */

    msr hcr_el2, xzr
    mov x10, #(1 << 31)      /* HCR_EL2.RW = 1: EL1 runs AArch64 */
    msr hcr_el2, x10
    msr cptr_el2, xzr
    msr sctlr_el1, xzr
    mov x10, #0x3c5          /* SPSR_EL2: EL1h, DAIF masked */
    msr spsr_el2, x10
    adr x10, 11f
    msr elr_el2, x10
    eret                     /* x0 (cpu index) survives */

11: mov x19, x0              /* cpu index, callee-saved from here */
    adr x10, 11b
    ldr x11, =11b
    sub x12, x11, x10        /* kimage_offset (per-core; same value) */

    /* AP boot stack at its PHYSICAL address (MMU still off). */
    ldr x0, =ap_boot_stacks
    sub x1, x19, #1          /* slot i-1 for core i */
    mov x2, #0x4000          /* 16 KiB per AP (see .bss below) */
    mul x1, x1, x2
    add x0, x0, x1
    add x0, x0, x2           /* top of my slot — SP 16-aligned */
    sub x0, x0, x12
    mov sp, x0

    mov x0, x19
    bl bootmmu_ap_enable     /* both TTBRs + caches — precedes any atomic */

    /* High VAs valid from here; abandon the low world. */
    ldr x0, =vectors
    msr vbar_el1, x0

    mov x0, sp
    add x0, x0, x12          /* same AP stack at its link VA */
    mov sp, x0

    mov x0, x19
    ldr x30, =secondary_main
    br x30                   /* high, never returns */

12: msr daifset, #0xf
13: wfe
    b 13b
.size secondary_entry, . - secondary_entry

/* Load/link delta (link - run): set by _start before .bss is cleared,
 * read forever after (kva.h's kva_to_pa uses it). */
.data
.global kimage_offset
kimage_offset:
    .xword 0

/* Per-AP boot stacks: slots 0..MAX_CPUS-2 for cores 1..MAX_CPUS-1. .bss
 * (zeroed by the BSP's _start before any CPU_ON), 16-byte aligned, sizes
 * a multiple of 16 so every slot top stays SP-aligned. */
.section .bss
.global ap_boot_stacks
.p2align 4
ap_boot_stacks:
    .space (16 - 1) * 0x4000
ap_boot_stacks_end:
