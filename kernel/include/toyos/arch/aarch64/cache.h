/*
 * cache.h — I-cache maintenance (aarch64)
 *
 * The I-cache is not coherent with the D-cache: any generated code (the
 * R4 llvmpipe JIT, module loaders) must be cleaned from D-cache and
 * invalidated in I-cache before execution — the aarch64 replacement for
 * x86's implicit coherence. Line sizes come from CTR_EL0 (ID registers),
 * never hardcoded.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_CACHE_H
#define TOYOS_ARCH_AARCH64_CACHE_H

#include <toyos/kernel/types.h>

/*
 * arch_clear_cache - make writes to [start, end) executable on this core.
 * dc cvau per D-cache line -> ic ivau per I-cache line -> dsb + isb, the
 * blueprint's non-negotiable sequence (same contract as __clear_cache).
 */
void arch_clear_cache(void* start, void* end);

#endif /* TOYOS_ARCH_AARCH64_CACHE_H */
