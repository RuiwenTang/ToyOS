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

    /* Park vector table: with VBAR_EL1 at its reset value (0x0) any
     * exception lands on the boot stub and the kernel re-runs — which
     * looks on the serial console like overlapping garbage output. Point
     * VBAR_EL1 at 16 slots that spin in place; nothing escapes to 0x0.
     * (The real per-class vector stubs arrive with R1.) */
    adr x0, vectors_park
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

/* VBAR_EL1 table: 16 slots × 128 bytes (SP_EL0/SP_ELx, 4 classes ×
 * current/LOWER_AARCH64), every slot an infinite spin. */
.section .vectors, "ax"
vectors_park:
.rept 16
    b .                      /* park: exceptions must not escape */
    .space 124
.endr
