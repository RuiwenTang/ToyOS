/*
 * cache.c — I-cache maintenance (aarch64)
 *
 * See cache.h. Line sizes are read from CTR_EL0 (bits 19:16 DMinLine,
 * bits 3:0 IMinLine — log2(words per line) × 2 = log2(bytes)); both QEMU
 * and the A55 use 64-byte lines, but the point is to never assume it.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cache.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/types.h>

static uint64_t ctr_line_size(uint64_t ctr, int shift) {
  /* words-per-line = 4 << field; bytes = words * 4 */
  return 4ull << ((ctr >> shift) & 0xf);
}

void arch_clear_cache(void* start, void* end) {
  uint64_t ctr = sysreg_read(CTR_EL0);
  uint64_t dline = ctr_line_size(ctr, 16);
  uint64_t iline = ctr_line_size(ctr, 0);
  uintptr_t s = (uintptr_t)start;
  uintptr_t e = (uintptr_t)end;

  s &= ~(dline - 1);
  for (uintptr_t p = s; p < e; p += dline)
    __asm__ __volatile__("dc cvau, %0" ::"r"(p) : "memory");

  __asm__ __volatile__("dsb ish" ::: "memory");

  for (uintptr_t p = s; p < e; p += iline)
    __asm__ __volatile__("ic ivau, %0" ::"r"(p) : "memory");

  __asm__ __volatile__(
      "dsb ish\n"
      "isb":::
          "memory");
}
