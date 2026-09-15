/*
 * spinlock.h — SMP-first spinlock primitive (aarch64)
 *
 * Two acquire flavours, deliberately split (same split and rationale as
 * ToyOS64):
 *
 *   - spin_lock / spin_unlock          plain test-and-set; leaves DAIF.I
 *                                      alone. Use only where the caller
 *                                      does not need IRQ exclusion.
 *
 *   - spin_lock_irqsave /              irq_save() first, so the holder and
 *     spin_unlock_irqrestore             the contended spin loop both run with
 *                                      DAIF.I = 0 (a timer tick can never
 *                                      preempt a holder, and an IRQ handler
 *                                      can never re-enter a held lock). The
 *                                      saved DAIF is stored inside the lock.
 *
 * Correctness invariant (irqsave path): every successful acquire returns with
 * IRQs masked, and the matching restore returns DAIF to the value seen at
 * acquire. Call sites must release before scheduling (never schedule inside
 * a critical section) so a holder always reaches release.
 *
 * The saved DAIF lives in the lock, not on the caller's stack: only the
 * winning acquire writes it (the losing spin path does not), so there is no
 * clobber race between contended acquirers.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_SPINLOCK_H
#define TOYOS_KERNEL_SPINLOCK_H

#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/types.h>

/* --- Arch primitives (aarch64) --- */

/*
 * xchg32 - Atomically exchange a 32-bit value.
 *
 * An acquire exchange: the LDXR/STXR (or SWPA) expansion the builtin lowers
 * to orders subsequent loads/stores after the lock word is observed free.
 *
 * @addr:    Pointer to the memory location
 * @newval:  Value to store
 * Returns:  The previous value at *addr
 */
static inline uint32_t xchg32(volatile uint32_t* addr, uint32_t newval) {
  return __atomic_exchange_n(addr, newval, __ATOMIC_ACQUIRE);
}

/*
 * irq_save - Mask IRQs and return the previous DAIF.
 *
 * DAIF is the aarch64 RFLAGS.IF analogue (one bit per exception class; only
 * the I bit matters here — nothing in the design is Group 0/FIQ). The
 * returned value is passed to irq_restore() to restore the original state,
 * exactly like x86's pushfq/cli/popfq triple.
 */
static inline uint64_t irq_save(void) {
  uint64_t daif = sysreg_read(DAIF);
  __asm__ volatile("msr daifset, #2" ::: "memory");
  return daif;
}

/*
 * irq_restore - Restore interrupt state from a saved DAIF value.
 */
static inline void irq_restore(uint64_t daif) { sysreg_write(DAIF, daif); }

/* --- Spinlock type --- */

typedef struct {
  volatile uint32_t locked; /* 0 = unlocked, 1 = locked */
  uint64_t int_flags;       /* saved DAIF for irq_restore (irqsave path) */
} spinlock_t;

/* --- Spinlock API --- */

/*
 * spin_lock_init - Initialize a spinlock to the unlocked state.
 */
static inline void spin_lock_init(spinlock_t* lock) {
  lock->locked = 0;
  lock->int_flags = 0;
}

/* Plain acquire — no IRQ handling. Spins (DAIF unchanged) until locked == 0. */
void spin_lock(spinlock_t* lock);

/* Release: release-order store of locked = 0 (see spinlock.c). */
void spin_unlock(spinlock_t* lock);

/* IRQ-saving acquire — masks interrupts across the critical section. */
void spin_lock_irqsave(spinlock_t* lock);

/* Restore locked = 0 then restore the DAIF saved at acquire. */
void spin_unlock_irqrestore(spinlock_t* lock);

/*
 * Legacy aliases (develop compatibility, same as ToyOS64 — the copied
 * sched/mm callers use these names; they map to the irqsave path).
 */
static inline void spinlock_init(spinlock_t* lock) { spin_lock_init(lock); }

static inline void spinlock_acquire(spinlock_t* lock) {
  spin_lock_irqsave(lock);
}

static inline void spinlock_release(spinlock_t* lock) {
  spin_unlock_irqrestore(lock);
}

#endif /* TOYOS_KERNEL_SPINLOCK_H */
