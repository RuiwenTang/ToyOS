/*
 * atomic.h — Atomic primitives (aarch64 kernel + host, one path)
 *
 * Same API as ToyOS64's atomic.h. The upstream kernel path is hand-written
 * lock-prefix x86 asm; on aarch64 the __atomic_* compiler builtins are the
 * port — per the blueprint's memory-model policy they lower correctly for
 * the target (LDXR/STXR + proper fences), so kernel and host now share one
 * implementation and the host unit tests exercise exactly what the kernel
 * runs.
 *
 * All operations are SEQ_CST, matching both the x86 lock-prefix semantics
 * upstream relied on and its C11 host fallback.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_ATOMIC_H
#define TOYOS_KERNEL_ATOMIC_H

#include <stdbool.h>
#include <toyos/kernel/types.h>

static inline void atomic_inc(volatile uint32_t* p) {
  __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

static inline void atomic_dec(volatile uint32_t* p) {
  __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST);
}

/*
 * atomic_add_return - Atomically add @a to *p and return the new value.
 */
static inline uint32_t atomic_add_return(volatile uint32_t* p, uint32_t a) {
  return __atomic_add_fetch(p, a, __ATOMIC_SEQ_CST);
}

/*
 * atomic_cas - Compare-and-swap: if *p == old, store neu, return true.
 */
static inline bool atomic_cas(volatile uint32_t* p, uint32_t old,
                              uint32_t neu) {
  return __atomic_compare_exchange_n(p, &old, neu, false, __ATOMIC_SEQ_CST,
                                     __ATOMIC_SEQ_CST);
}

/*
 * atomic_cas64 - 64-bit compare-and-swap (for SMP page-table-entry updates
 * once paging lands in R2). Same contract as atomic_cas, quad-word, for
 * CoW/protect PTE rewrites that must be atomic against another core's CoW
 * fault on the same VA.
 */
static inline bool atomic_cas64(volatile uint64_t* p, uint64_t old,
                                uint64_t neu) {
  return __atomic_compare_exchange_n(p, &old, neu, false, __ATOMIC_SEQ_CST,
                                     __ATOMIC_SEQ_CST);
}

/*
 * atomic_load/store - Aligned volatile access with a compiler barrier, so
 * surrounding accesses cannot sink past / float ahead of the access.
 */
static inline uint32_t atomic_load(const volatile uint32_t* p) {
  return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

static inline void atomic_store(volatile uint32_t* p, uint32_t v) {
  __atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}

#endif /* TOYOS_KERNEL_ATOMIC_H */
