/*
 * refcount.h — Unified reference counter (smp-first, Phase 2b-mm)
 *
 * All kernel lifetimes (pmm page, PCB, thread) go through refcount_t instead
 * of bare __atomic on scattered ints. Centralises the underflow guard (the
 * ToyOS64 lesson): a balanced refcount never decs below 0, so refcount_put
 * undoes a dec that would make it negative rather than returning 0 and letting
 * the caller free a still-referenced object. Returns whether the count hit 0.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_REFCOUNT_H
#define TOYOS_KERNEL_REFCOUNT_H

#include <toyos/kernel/types.h>

struct refcount_t {
  volatile int count;
};

static inline void refcount_init(struct refcount_t* r, int v) {
  __atomic_store_n(&r->count, v, __ATOMIC_RELEASE);
}

/* Inc. Returns the new count (callers usually discard it). */
static inline int refcount_get(struct refcount_t* r) {
  return __atomic_add_fetch(&r->count, 1, __ATOMIC_SEQ_CST);
}

/*
 * Try-inc. Like refcount_get but never revives a dead object: increments only
 * if the current count is > 0. Returns true on success. Used by walkers (the
 * pager's evict-by-unmap scan) that must pin an object while holding the list
 * lock, without resurrecting one that another core just dropped to 0.
 */
static inline bool refcount_try_get(struct refcount_t* r) {
  int c = __atomic_load_n(&r->count, __ATOMIC_SEQ_CST);
  while (c > 0) {
    if (__atomic_compare_exchange_n(&r->count, &c, c + 1, false,
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
      return true;
  }
  return false;
}

/*
 * Dec. Returns true iff the count hit 0 (caller should release). Guards
 * underflow: if the count is already <= 0 (a bug — more puts than gets), undoes
 * the dec and returns false so the caller does NOT free a still-referenced
 * object.
 */
static inline bool refcount_put(struct refcount_t* r) {
  int prev = __atomic_fetch_sub(&r->count, 1, __ATOMIC_SEQ_CST);
  if (prev <= 0) {
    __atomic_add_fetch(&r->count, 1, __ATOMIC_SEQ_CST); /* undo */
    return false; /* underflow — do not release */
  }
  return prev == 1; /* hit 0 */
}

static inline int refcount_read(struct refcount_t* r) {
  return __atomic_load_n(&r->count, __ATOMIC_SEQ_CST);
}

#endif /* TOYOS_KERNEL_REFCOUNT_H */
