/*
 * trap.h — exception frame + vector slot numbers (aarch64)
 *
 * The trap_frame mirrors the push order in traps.S exactly; the assembly
 * computes offsets by hand, so any layout change here needs a matching
 * change there. x86 analogue: the iret frame the CPU pushes, except ARM
 * hardware gives us only ELR/SPSR — everything else is software-saved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_TRAP_H
#define TOYOS_ARCH_AARCH64_TRAP_H

/* Vector slot numbers — the quadrant they came from is part of the trap,
 * because "sync at current EL" and "sync from EL0" can be different bugs
 * even when ESR is identical. Shared with traps.S; keep them #defines. */
#define TRAP_SYNC_CUR_SP0 0
#define TRAP_IRQ_CUR_SP0 1
#define TRAP_FIQ_CUR_SP0 2
#define TRAP_SERROR_CUR_SP0 3

#define TRAP_SYNC_CUR_SPX 4
#define TRAP_IRQ_CUR_SPX 5
#define TRAP_FIQ_CUR_SPX 6
#define TRAP_SERROR_CUR_SPX 7

#define TRAP_SYNC_LOW_A64 8
#define TRAP_IRQ_LOW_A64 9
#define TRAP_FIQ_LOW_A64 10
#define TRAP_SERROR_LOW_A64 11

#define TRAP_SYNC_LOW_A32 12
#define TRAP_IRQ_LOW_A32 13
#define TRAP_FIQ_LOW_A32 14
#define TRAP_SERROR_LOW_A32 15

#ifndef __ASSEMBLER__

#include <toyos/kernel/types.h>

/* As pushed by trap_entry_asm, from low address to high. Above the struct
 * (+304) sits one more 16-byte staging pair the vector slot pushed: the
 * interrupted context's ORIGINAL x30, stashed before the slot loaded its
 * number into x30 — the shared entry patches it into x[29] (the x30 slot).
 * C never sees the staging pair; the epilogue drops it. */
struct trap_frame {
  uint64_t sp_el0; /* +0 */
  uint64_t pad0;   /* +8 */
  uint64_t elr;    /* +16 return PC for eret */
  uint64_t spsr;   /* +24 PSTATE to restore */
  uint64_t x0;     /* +32 */
  uint64_t pad1;   /* +40 stp needs pairs; 31 GPRs is odd */
  uint64_t x[30];  /* +48 x1..x30 */
  uint64_t class;  /* +288 vector slot number */
  uint64_t pad2;
};

/* C dispatcher invoked by trap_entry_asm; the vector slot number is
 * f->class (the stub's x30, pushed at the top of the frame). */
void trap_dispatch(struct trap_frame* f);

#endif /* __ASSEMBLER__ */
#endif /* TOYOS_ARCH_AARCH64_TRAP_H */
