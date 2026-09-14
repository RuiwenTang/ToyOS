/*
 * traps.S — VBAR_EL1 exception vector table + context save/restore
 *
 * The x86 analogue is the IDT plus its stubs, but the shape is different:
 * one table of 16 slots × 128 bytes, 2 KiB aligned, selected by
 * (taken-from EL & SP) × (exception class). No IST/TSS — an exception from
 * EL0 lands on SP_EL1 whatever it holds, so the kernel stack must be
 * per-thread by the time ring 3 exists (R2); today it is the boot stack.
 *
 * Slots cannot hold a full save sequence (30 registers), so each one
 * carries only its slot number in x30 and jumps to the shared entry, which
 * pushes a trap_frame and calls the C dispatcher. x30 is call-clobbered
 * scratch here, exactly like using rcx to pass the vector number in an
 * x86 hand-rolled stub.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/trap.h>

/* void trap_entry_asm(u64 class_in_x30) — shared tail of all 16 slots. */
.section .text
.type trap_entry_asm, @function
trap_entry_asm:
    /* The slot number still sits in x30; stash it below the frame first. */
    stp x30, xzr, [sp, #-16]!

    /* x0..x29, with x30/LR (stashed class) re-saved in frame order. */
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!
    stp x13, x14, [sp, #-16]!
    stp x11, x12, [sp, #-16]!
    stp x9, x10, [sp, #-16]!
    stp x7, x8, [sp, #-16]!
    stp x5, x6, [sp, #-16]!
    stp x3, x4, [sp, #-16]!
    stp x1, x2, [sp, #-16]!
    stp x0, xzr, [sp, #-16]!

    mrs x1, sp_el0
    mrs x2, elr_el1
    mrs x3, spsr_el1
    stp x2, x3, [sp, #-16]!        /* elr, spsr */
    stp x1, xzr, [sp, #-16]!       /* sp_el0, (pad) */

    mov x0, sp                     /* frame — trap_dispatch reads f->class */
    bl trap_dispatch

    ldp x1, xzr, [sp], #16         /* sp_el0 */
    ldp x2, x3, [sp], #16          /* elr, spsr */
    msr elr_el1, x2
    msr spsr_el1, x3
    msr sp_el0, x1

    ldp x0, xzr, [sp], #16
    ldp x1, x2, [sp], #16
    ldp x3, x4, [sp], #16
    ldp x5, x6, [sp], #16
    ldp x7, x8, [sp], #16
    ldp x9, x10, [sp], #16
    ldp x11, x12, [sp], #16
    ldp x13, x14, [sp], #16
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16        /* x30 = original LR again */
    add sp, sp, #16                /* drop stashed class */

    eret
.size trap_entry_asm, . - trap_entry_asm

/*
 * The 16 slots. Quadrants (rows) are the SP/EL the exception was taken
 * from; columns are the class. LOWER_EL quadrants stay parked until ring 3
 * exists (R2) — nothing can take from EL0 before then.
 */
.section .vectors, "ax"
.global vectors
.type vectors, @function
vectors:
    /* current EL, SP_EL0 (never entered today: kernel runs SP_EL1) */
    mov x30, #TRAP_SYNC_CUR_SP0
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_IRQ_CUR_SP0
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_FIQ_CUR_SP0
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_SERROR_CUR_SP0
    b trap_entry_asm
    .space 120

    /* current EL, SP_ELx — where all R1 exceptions arrive */
    mov x30, #TRAP_SYNC_CUR_SPX
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_IRQ_CUR_SPX
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_FIQ_CUR_SPX
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_SERROR_CUR_SPX
    b trap_entry_asm
    .space 120

    /* lower EL, AArch64 — ring 3 entries, live from R2 */
    mov x30, #TRAP_SYNC_LOW_A64
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_IRQ_LOW_A64
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_FIQ_LOW_A64
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_SERROR_LOW_A64
    b trap_entry_asm
    .space 120

    /* lower EL, AArch32 — 32-bit userland is out of scope forever */
    mov x30, #TRAP_SYNC_LOW_A32
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_IRQ_LOW_A32
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_FIQ_LOW_A32
    b trap_entry_asm
    .space 120
    mov x30, #TRAP_SERROR_LOW_A32
    b trap_entry_asm
    .space 120
.size vectors, . - vectors
