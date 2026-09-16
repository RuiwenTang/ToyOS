/*
 * bootmmu.c — boot-time identity map (aarch64): make RAM Normal Cacheable
 *
 * Why this exists ahead of the R2 paging port: with the MMU off, all memory
 * is Device type, and Device memory does not support atomic or exclusive
 * operations — LDAXR/STXR/LDADD raise DFSC 0x35 (alignment, HVF) or 0x61
 * (unsupported atomic hardware update, TCG). The scheduler port (spinlocks,
 * refcounts, TID assignment) is built on atomics, and unlike x86 there is no
 * "MMU off but cacheable" mode — the tables must go on. First hit: heap_lock,
 * the very first spin_lock_irqsave.
 *
 * Scope, deliberately minimal (the R2 design — TTBR0/TTBR1 split, user
 * spaces, nG/AF/ASID discipline, CoW — is untouched by this):
 *   - two static tables in .bss: L0 + one L1
 *   - VA [0, 0x40000000)  → Device-nGnRE 1 GB blocks (all MMIO: GIC, PL011)
 *   - VA [0x40000000, 0x80000000) → Normal WB 1 GB block (QEMU virt's RAM
 *     base; RK3568 RAM starts at the same address)
 *   - everything else unmapped (faults — correct)
 *
 * Identity throughout (VA == PA), so enabling the MMU changes memory types,
 * not addresses. fdt.c's volatile byte loads (the Device-merge workaround)
 * stay correct, just no longer load-bearing.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* Descriptor types, bits[1:0] (same encoding as Linux's PMD_TYPE_*):
 * 0b01 = block/section (L1/L2), 0b11 = table (L0-L2) or page (L3).
 * Attribute indices match the MAIR_EL1 programming below. SH=11
 * inner-shareable, AF=1 (access flag — never fault), the rest RWX at
 * EL1/EL0 (no permissions at boot). */
#define DESC_BLOCK (1u << 0)
#define DESC_TABLE (3u << 0)

#define ATTR_DEVICE 0x0 /* MAIR attr 0: Device-nGnRE */
#define ATTR_NORMAL 0x1 /* MAIR attr 1: Normal WB RW */

#define BLOCK_DESC(pa_base, attr)                                           \
  ((((uint64_t)(pa_base)) & 0x0000ffffffe00000ull) /* bits[47:21], 2 MiB */ \
   | DESC_BLOCK                                    /* bits[1:0]        */   \
   | (((uint64_t)(attr)) << 2)                     /* AttrIndx         */   \
   | (1u << 10)                                    /* AF               */   \
   | (3u << 8))                                    /* SH               */

/* Standard 3-level shape (the same one U-Boot/Linux boot on): L0 → L1
 * table → L2 2 MiB blocks. */
static uint64_t boot_l0[512] __attribute__((aligned(4096)));
static uint64_t boot_l1[512] __attribute__((aligned(4096)));
static uint64_t boot_l2_mmio[512] __attribute__((aligned(4096)));
static uint64_t boot_l2_ram[512] __attribute__((aligned(4096)));

static uint64_t table_desc(uint64_t* table) {
  return (uint64_t)(uintptr_t)table | DESC_TABLE;
}

void bootmmu_init(void) {
  /* [0, 1 GB) MMIO → Device; [1 GB, 2 GB) = 0x40000000.. → RAM → Normal.
   * L2 blocks are 2 MiB: one L2 table per 1 GiB L1 slot. */
  for (int i = 0; i < 512; i++) {
    boot_l2_mmio[i] = BLOCK_DESC((uint64_t)i << 21, ATTR_DEVICE);
    boot_l2_ram[i] =
        BLOCK_DESC(0x40000000ull + ((uint64_t)i << 21), ATTR_NORMAL);
  }
  boot_l1[0] = table_desc(boot_l2_mmio);
  boot_l1[1] = table_desc(boot_l2_ram);
  boot_l0[0] = table_desc(boot_l1);

  /* MAIR: attr 0 = Device-nGnRE, attr 1 = Normal WB RW-cacheable. */
  const uint64_t mair = ((uint64_t)0x04 << 0) | ((uint64_t)0xff << 8);
  sysreg_write(MAIR_EL1, mair);

  /* TCR_EL1: 48-bit VA (T0SZ=16), 4 KiB granule, 40-bit PA (IPS=2 — covers
   * QEMU virt and the RK3568's 36-bit), inner-shareable WB walk. TTBR1 is
   * untouched (R2). */
  const uint64_t tcr = (16u << 0)     /* T0SZ */
                       | (0u << 14)   /* TG0 = 4 KiB */
                       | (3u << 12)   /* SH0 inner-shareable */
                       | (1u << 10)   /* ORGN0 write-back */
                       | (1u << 8)    /* IRGN0 write-back */
                       | (2ull << 32) /* IPS = 40-bit */
                       | (16u << 16); /* T1SZ (unused, mirrors T0) */
  sysreg_write(TCR_EL1, tcr);
  sysreg_write(TTBR0_EL1, (uint64_t)(uintptr_t)boot_l0);

  barrier_dsb_sy(); /* tables + sysregs land before the MMU reads them */
  barrier_isb();

  uint64_t sctlr = sysreg_read(SCTLR_EL1);
  sctlr |= (1u << 0)     /* M: MMU on */
           | (1u << 2)   /* C: data cache */
           | (1u << 12); /* I: instruction cache */
  sysreg_write(SCTLR_EL1, sctlr);
  barrier_isb();

  serial_puts("mmu: boot identity map on — RAM Normal WB, MMIO Device\n");
}
