/*
 * kva.h — kernel virtual address layout (R2.2, see blueprint)
 *
 * 4 KiB granule, 48-bit VA (T0SZ = T1SZ = 16), 4 levels. TTBR1 owns the
 * top half and is loaded at boot, never switched:
 *
 *   0xFFFF8000_00000000  direct map (L0 slot 256, 512 GiB PA window)
 *   0xFFFFFFF0_00000000  kstack region   (slot 511, L1 #508)
 *   0xFFFFFFFF_40000000  kernel heap     (slot 511, L1 #509, R2.4)
 *   0xFFFFFFFF_80000000  kernel image    (slot 511, L1 #510, link base)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_ARCH_AARCH64_KVA_H
#define TOYOS_ARCH_AARCH64_KVA_H

#include <toyos/kernel/types.h>

/* Direct map: va = pa + KERNEL_DM_BASE, PA window [0, 512 GiB) — covers
 * every RAM bank and MMIO PA on both targets (QEMU virt keeps MMIO below
 * 1 GiB; the RK3568 tops out at 8 GiB DRAM + peripherals below 4 GiB). */
#define KERNEL_DM_BASE 0xFFFF800000000000ull
#define KERNEL_DM_WINDOW 0x8000000000ull /* 512 GiB of PA */

/* Kernel scratch lives in TTBR1 L0 slot 511, one 1 GiB L1 slot each. */
#define KERNEL_KSTACK_BASE 0xFFFFFFFF00000000ull
#define KERNEL_KSTACK_SIZE (16ull * 1024 * 1024) /* grow on demand */
#define KERNEL_HEAP_BASE 0xFFFFFFFF40000000ull /* ToyOS64's constant */
#define KERNEL_HEAP_SIZE (1ull << 30)          /* mapped from R2.4 */
#define KERNEL_IMAGE_BASE 0xFFFFFFFF80000000ull /* link base +0x40 */

/* Image load-vs-link offset, computed once by entry.S: for LINK VAs
 * inside the kernel image, va = pa + kimage_offset. Distinct from the
 * direct map (which serves every other PA).
 *
 * ⚠ Only pass LINK VAs: a C symbol reference (`&obj`, `__kernel_start`)
 * compiles to ADRP/ADR, which resolves against the RUNNING PC — at the
 * low world it already yields the physical address, so subtracting
 * kimage_offset would double-count. Low-world code derives the image PA
 * from the KERNEL_IMAGE_BASE constant instead (see bootmmu.c /
 * kmain_low); at the link VA every symbol is its link VA and this is
 * exact. */
extern uint64_t kimage_offset;

static inline uintptr_t kva_dm(uintptr_t pa) {
  return pa + KERNEL_DM_BASE;
}

static inline uintptr_t kva_to_pa(uintptr_t image_link_va) {
  return image_link_va - kimage_offset;
}

static inline uintptr_t kva_pa_to_image_va(uintptr_t pa) {
  return pa + kimage_offset;
}

#endif /* TOYOS_ARCH_AARCH64_KVA_H */
