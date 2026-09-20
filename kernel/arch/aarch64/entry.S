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

1:  /* EL1, C calling convention from here on. */
    mov x19, x0              /* preserve DTB pointer across setup */
    ldr x0, =__stack_top
    mov sp, x0

    /* VBAR_EL1 → the real 16-slot table in traps.S. Must be armed before
     * anything can trap (bss clearing, C code). The literal-pool form
     * works at any link distance — adr's ±4 KiB range would silently
     * depend on .vectors staying right after .text.boot in the linker
     * script (it does today, but not as a contract anyone should rely
     * on). Its 2 KiB alignment is the linker script's job. */
    ldr x0, =vectors
    msr vbar_el1, x0

    /* Zero .bss — RAM contents are whatever the loader left behind. */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
3:  cmp x0, x1
    b.hs 4f
    str xzr, [x0], #8
    b 3b

4:  mov x0, x19
    bl kmain

2:  /* kmain returned (it must not) or unsupported EL: park. */
    msr daifset, #0xf
5:  wfe
    b 5b

.size _start, . - _start

/*
 * secondary_entry — PSCI CPU_ON target for cores 1..N-1.
 *
 * Entry state (PSCI contract): x0 = context_id (this core's cpu_locals
 * slot), MMU off, caches off, IRQs masked, and — on QEMU — EL2, exactly
 * the boot contract _start sees; the settle sequence below is the same
 * one verbatim. The order after that is the SMP-critical part:
 *
 *   stack → VBAR → bootmmu_ap_enable → secondary_main
 *
 * bootmmu_ap_enable (same shared identity tables as the BSP) must run
 * before ANY atomic/exclusive: with the MMU off all memory is Device,
 * and LDAXR/STXR fault there (DFSC 0x35 — the heap_lock lesson, now per
 * core). C code before the MMU is fine as long as it is atomic-free.
 *
 * The 16 KiB boot stack is abandoned when secondary_main swaps onto the
 * idle thread's guarded kstack (the sched_start discipline).
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
    ldr x0, =ap_boot_stacks
    sub x1, x19, #1          /* slot i-1 for core i */
    mov x2, #0x4000          /* 16 KiB per AP (see .bss below) */
    mul x1, x1, x2
    add x0, x0, x1
    add x0, x0, x2           /* top of my slot — SP 16-aligned */
    mov sp, x0

    ldr x0, =vectors
    msr vbar_el1, x0

    mov x0, x19
    bl bootmmu_ap_enable     /* MMU + caches on — precedes any atomic */
    mov x0, x19
    bl secondary_main        /* never returns */

12: msr daifset, #0xf
13: wfe
    b 13b
.size secondary_entry, . - secondary_entry

/* Per-AP boot stacks: slots 0..MAX_CPUS-2 for cores 1..MAX_CPUS-1. .bss
 * (zeroed by the BSP's _start before any CPU_ON), 16-byte aligned, sizes
 * a multiple of 16 so every slot top stays SP-aligned. */
.section .bss
.global ap_boot_stacks
.p2align 4
ap_boot_stacks:
    .space (16 - 1) * 0x4000
ap_boot_stacks_end:
