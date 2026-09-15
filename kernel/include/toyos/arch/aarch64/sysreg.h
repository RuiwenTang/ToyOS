/*
 * sysreg.h — system register access + barriers (aarch64)
 *
 * One macro pair instead of scattered inline asm; string-pasting the
 * register name keeps clang's asm validator in the loop (a typo in a
 * register name is a build error, not a runtime trap to the vector
 * dispatcher).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_SYSREG_H
#define TOYOS_ARCH_AARCH64_SYSREG_H

#include <toyos/kernel/types.h>

/* Read a system register by name: uint64_t v = sysreg_read(CNTVCT_EL0); */
#define sysreg_read(reg)                              \
  ({                                                  \
    uint64_t v_;                                      \
    __asm__ __volatile__("mrs %0, " #reg : "=r"(v_)); \
    v_;                                               \
  })

/* Write a system register by name. "memory" so config writes (CNTV_*,
 * ICC_*) cannot sink past later code. */
#define sysreg_write(reg, val) \
  __asm__ __volatile__("msr " #reg ", %0" : : "r"((uint64_t)(val)) : "memory")

/* Completion/synchronization barriers — the blueprint's Risks section:
 * weak memory ordering is a new failure class vs x86, so config paths
 * carry their barriers explicitly. */
static inline void barrier_dsb_sy(void) {
  __asm__ __volatile__("dsb sy" ::: "memory");
}

static inline void barrier_isb(void) {
  __asm__ __volatile__("isb" ::: "memory");
}

#endif /* TOYOS_ARCH_AARCH64_SYSREG_H */
