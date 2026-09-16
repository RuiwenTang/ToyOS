/*
 * switch.S — Cooperative context switch (aarch64)
 *
 * The x86_64 original saves the callee-saved set (rbp, rbx, r12-r15) plus
 * the return address; the aarch64 callee-saved set is x19-x28 plus x29 (FP)
 * and x30 (LR). x30 doubles as the resume PC: it is the stacked return
 * address on a preempted thread (inside schedule(), right after its
 * switch_to call) and thread_trampoline on a fresh one, exactly the role
 * the stacked 'ret target' plays upstream.
 *
 * Fresh threads never execute schedule()'s resume side: stack_frame_init
 * (sched.c) lays out the 12-qword frame with x19=arg, x20=entry and
 * x30=thread_trampoline, so the first switch 'returns' straight into the
 * trampoline, which releases run_lock via sched_post_switch before calling
 * entry(arg). The register contract (x19/x20) mirrors upstream's rbx/r12.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

.section .text

/*
 * switch_to(uint64_t *current_sp, uint64_t *next_sp)
 *   x0 = &curr->kernel_rsp, x1 = &next->kernel_rsp
 *
 * Callee-saved-only, per the x86 rationale verbatim: caller-saved registers
 * are live only inside schedule()'s C frame, which owns saving them (the
 * trap frame on a preempted thread, the initial frame on a fresh one).
 *
 * Six STP/LDP pairs (96 bytes) keep SP 16-byte aligned at every step, per
 * AAPCS64 — the aarch64 stand-in for upstream's manual stack alignment.
 */
.globl switch_to
switch_to:
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!

    /* SP cannot be a str/ldr data operand — go through the IP scratch
     * registers (caller-saved, so not part of the switched context). */
    mov x16, sp
    str x16, [x0]         /* curr->kernel_rsp = sp */
    ldr x17, [x1]         /* x17 = next->kernel_rsp */
    mov sp, x17

    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16
    ret                   /* to schedule()'s resume side, or thread_trampoline */

/*
 * thread_trampoline - First instruction of every new thread (reached via
 * switch_to's ret with x19=arg, x20=entry). Release the run_lock acquired
 * by whichever CPU's schedule() switched here, unmask IRQs (schedule runs
 * with them masked — without this the timer could never preempt the new
 * thread), then call entry(arg). If entry returns, park with IRQs enabled
 * so the tick still fires (same reasoning as upstream's sti;hlt loop).
 */
.globl thread_trampoline
thread_trampoline:
    bl    sched_post_switch
    mov   x0, x19
    blr   x20
    msr   daifclr, #2
1:  wfi
    b     1b

.type switch_to, @function
.type thread_trampoline, @function
