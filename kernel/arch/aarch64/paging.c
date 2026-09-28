/*
 * paging.c — kernel-tree page-table walker (TTBR1, R2.2)
 *
 * Walks the static kernel root at 4 KiB granularity; missing intermediate
 * tables come from the pmm (zeroed — all-invalid — before linking in,
 * break-before-make discipline for anything that was valid). Descriptor
 * encodings match bootmmu.c. The R2.4 vmm.c port reuses this walker for
 * the kernel tree and clones it for user trees.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/kva.h>
#include <toyos/arch/aarch64/paging.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/pmm.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/string.h>
#include <toyos/kernel/types.h>

#define DESC_TABLE (3u << 0)

uint64_t* paging_kernel_root(void); /* bootmmu.c */

/* Descriptors hold PHYSICAL table addresses (hardware walks PAs); the
 * kernel runs at high VAs, so every dereference goes through the direct
 * map. Uniform for static .bss tables (built while running low, where
 * VA == PA, so their stored descriptors are PAs) and pmm-allocated ones. */
static uint64_t* table_va(uint64_t desc) {
  return (uint64_t*)(uintptr_t)((desc & 0x0000fffffffff000ull) +
                                KERNEL_DM_BASE);
}

/* Table-edit sequence (blueprint): edit -> dsb ish -> tlbi -> dsb ish ->
 * isb — non-negotiable on unmap; map of a never-valid VA needs only the
 * first dsb (other cores may walk concurrently). */

int kernel_map_page(uintptr_t va, uintptr_t pa, uint64_t flags) {
  uint64_t* l0 = paging_kernel_root();
  uint64_t* l1;
  uint64_t* l2;
  uint64_t* l3;

  if (va & 0xfff || pa & 0xfff) return -1;

  uint64_t d = l0[(va >> 39) & 0x1FF];
  if ((d & 3) != DESC_TABLE) return -1; /* root slots are pre-linked only */
  l1 = table_va(d);

  d = l1[(va >> 30) & 0x1FF];
  if ((d & 3) != DESC_TABLE) {
    uintptr_t phys = pmm_alloc();
    if (!phys) return -1;
    l2 = (uint64_t*)pmm_phys_to_virt(phys);
    memset(l2, 0, 4096);
    /* descriptors hold PHYSICAL table addresses — the DM VA above is only
     * for our own writes; link the PA, dsb ish publishes it to others */
    l1[(va >> 30) & 0x1FF] = (uint64_t)phys | DESC_TABLE;
    __asm__ __volatile__("dsb ish" ::: "memory");
  } else {
    l2 = table_va(d);
  }

  d = l2[(va >> 21) & 0x1FF];
  if ((d & 3) != DESC_TABLE) {
    uintptr_t phys = pmm_alloc();
    if (!phys) return -1;
    l3 = (uint64_t*)pmm_phys_to_virt(phys);
    memset(l3, 0, 4096);
    l2[(va >> 21) & 0x1FF] = (uint64_t)phys | DESC_TABLE;
    __asm__ __volatile__("dsb ish" ::: "memory");
  } else {
    l3 = table_va(d);
  }

  l3[(va >> 12) & 0x1FF] =
      ((uint64_t)pa & 0x0000fffffffff000ull) | flags;
  __asm__ __volatile__("dsb ish" ::: "memory");
  return 0;
}

uintptr_t kernel_unmap_page(uintptr_t va) {
  uint64_t* l0 = paging_kernel_root();
  uint64_t* l1;
  uint64_t* l2;
  uint64_t* l3;
  uint64_t d;

  if (va & 0xfff) return 0;

  d = l0[(va >> 39) & 0x1FF];
  if ((d & 3) != DESC_TABLE) return 0;
  l1 = table_va(d);
  d = l1[(va >> 30) & 0x1FF];
  if ((d & 3) != DESC_TABLE) return 0;
  l2 = table_va(d);
  d = l2[(va >> 21) & 0x1FF];
  if ((d & 3) != DESC_TABLE) return 0;
  l3 = table_va(d);

  uint64_t pte = l3[(va >> 12) & 0x1FF];
  if ((pte & 1) == 0) return 0;

  l3[(va >> 12) & 0x1FF] = 0;
  __asm__ __volatile__(
      "dsb ish\n"
      "tlbi vaae1is, %0\n"
      "dsb ish\n"
      "isb"::"r"(va)
      : "memory");
  return (uintptr_t)(pte & 0x0000fffffffff000ull);
}
