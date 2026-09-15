/*
 * spinlock.c — SMP-first spinlock implementation (aarch64)
 *
 * Acquire spins on xchg32 (an acquire exchange, see spinlock.h) until the
 * lock is free. The irqsave flavour disables interrupts first, so the holder
 * and the contended spin loop run with DAIF.I = 0. Release is a real
 * release store — the x86 TSO argument for a plain store does not carry to
 * ARM's weakly-ordered memory model — and, for the irqsave flavour, restores
 * the DAIF observed at acquire.
 *
 * Correctness on SMP rests on one invariant: every successful irqsave
 * acquire returns with DAIF.I = 0, and the matching restore returns DAIF to
 * the value seen at acquire. A thread holding a lock therefore runs with
 * IRQs masked — a timer tick can never preempt it — and call sites always
 * do `unlock -> schedule()` (never schedule() inside the critical section),
 * so the holder always reaches release. A contended core spins with IRQs
 * masked exactly as hardware requires.
 *
 * The saved DAIF lives inside the lock, not on the caller's stack: only
 * the winning acquire writes it (the losing spin path does not), so there
 * is no clobber race between contended acquirers.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/spinlock.h>

void spin_lock(spinlock_t* lock) {
  for (;;) {
    if (xchg32(&lock->locked, 1) == 0) return; /* acquired; DAIF untouched */
    __asm__ volatile("yield");
  }
}

void spin_unlock(spinlock_t* lock) {
  /* Release store: on aarch64 a plain store is NOT release-ordered
   * (unlike x86 TSO) — the STLR semantics of the release store are what
   * keeps critical-section stores ordered before the unlock. */
  __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
}

void spin_lock_irqsave(spinlock_t* lock) {
  uint64_t flags = irq_save(); /* DAIF.I = 0 from here on */
  for (;;) {
    if (xchg32(&lock->locked, 1) == 0) {
      lock->int_flags = flags; /* only the winner writes */
      return;                  /* returns with IRQs masked */
    }
    __asm__ volatile("yield");
  }
}

void spin_unlock_irqrestore(spinlock_t* lock) {
  __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
  irq_restore(lock->int_flags);
}
