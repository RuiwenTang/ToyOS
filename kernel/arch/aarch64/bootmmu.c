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
 *   - two static tables in .bss: L0 + one L1, plus a pool of L2 tables
 *   - coverage: [0, max(4 GiB, align_up(ram_top, 1 GiB))) — every 2 MiB
 *     block inside a discovered RAM bank (memmap /memory walk) is Normal
 *     WB, everything else in range is Device-nGnRE (MMIO wherever the
 *     board put it: QEMU keeps it below 1 GiB; the RK3568 puts peripherals
 *     above DRAM at 0xFD000000+), and above the range nothing is mapped
 *     (faults — correct)
 *   - a 2 MiB block straddling a bank edge maps Device; both targets' RAM
 *     is 2 MiB-aligned so this cannot arise in practice, and if it ever
 *     does the edge pages fault loudly at their first atomic rather than
 *     corrupting silently
 *
 * Identity throughout (VA == PA), so enabling the MMU changes memory types,
 * not addresses. fdt.c's volatile byte loads (the Device-merge workaround)
 * stay correct, just no longer load-bearing.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/memmap.h>
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

#define L2_BYTES 4096u
#define BLOCK_BYTES (2u * 1024 * 1024)
#define L1_SLOT_BYTES (1u * 1024 * 1024 * 1024)

/* One L2 table per 1 GiB L1 slot in coverage. The 4 GiB floor needs 4;
 * 8 GiB of RK3568 DRAM (the ceiling target) reaches slot 9 → 10. The pool
 * covers RAM up to ~13 GiB; more than that is a compile-time bump. */
#define BOOT_L2_POOL 16

/* Standard 3-level shape (the same one U-Boot/Linux boot on): L0 → L1
 * table → L2 2 MiB blocks. */
static uint64_t boot_l0[512] __attribute__((aligned(4096)));
static uint64_t boot_l1[512] __attribute__((aligned(4096)));
static uint64_t boot_l2_pool[BOOT_L2_POOL][512] __attribute__((aligned(4096)));

static uint64_t table_desc(uint64_t* table) {
  return (uint64_t)(uintptr_t)table | DESC_TABLE;
}

/* Is [pa, pa+2 MiB) fully inside a discovered RAM bank? (Banks are sorted
 * and merged by memmap, but a plain scan is fine — this runs once.) */
static int block_is_ram(const mem_region_t* banks, size_t nbanks, uint64_t pa) {
  for (size_t i = 0; i < nbanks; i++) {
    if (pa >= banks[i].base && pa + BLOCK_BYTES <= banks[i].base + banks[i].len)
      return 1;
  }
  return 0;
}

/* Program MAIR/TCR/TTBR0 from the (already built) static tables and turn
 * the MMU + caches on. Shared by the BSP (after building the tables) and
 * every AP (tables are shared read-only after boot) — SCTLR/TCR/TTBR are
 * per-core registers, so each core must run this itself. Identity map
 * throughout: enabling the MMU changes memory types, never addresses. */
static void bootmmu_enable(void) {
  /* MAIR: attr 0 = Device-nGnRE, attr 1 = Normal WB RW-cacheable. */
  const uint64_t mair = ((uint64_t)0x04 << 0) | ((uint64_t)0xff << 8);
  sysreg_write(MAIR_EL1, mair);

  /* TCR_EL1: 48-bit VA (T0SZ=16), 4 KiB granule, 40-bit PA (IPS=2 — covers
   * QEMU virt and the RK3568's 36-bit), inner-shareable WB walk. TTBR1 is
   * untouched (R2.2). */
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
}

void bootmmu_init(const mem_region_t* banks, size_t nbanks) {
  /* Coverage: [0, max(4 GiB, align_up(ram_top, 1 GiB))). The 4 GiB floor
   * keeps the RK3568's above-DRAM peripherals mapped (they live below
   * 4 GiB); QEMU virt keeps all MMIO in slot 0 either way. */
  uint64_t ram_top = memmap_ram_top();
  uint64_t cover_end = 4ull * 1024 * 1024 * 1024;
  uint64_t ram_top_aligned =
      (ram_top + L1_SLOT_BYTES - 1) & ~(uint64_t)(L1_SLOT_BYTES - 1);
  if (ram_top_aligned > cover_end) cover_end = ram_top_aligned;

  size_t slots = (size_t)(cover_end / L1_SLOT_BYTES);
  if (slots > BOOT_L2_POOL) {
    serial_puts("mmu: coverage needs ");
    serial_print_dec(slots);
    serial_puts(" L2 tables, pool is ");
    serial_print_dec(BOOT_L2_POOL);
    serial_puts(" — bump BOOT_L2_POOL\n");
    cpu_halt();
  }

  for (size_t s = 0; s < slots; s++) {
    uint64_t* l2 = boot_l2_pool[s];
    for (uint32_t b = 0; b < 512; b++) {
      uint64_t pa = (uint64_t)s * L1_SLOT_BYTES + (uint64_t)b * BLOCK_BYTES;
      l2[b] = block_is_ram(banks, nbanks, pa) ? BLOCK_DESC(pa, ATTR_NORMAL)
                                              : BLOCK_DESC(pa, ATTR_DEVICE);
    }
    boot_l1[s] = table_desc(l2);
  }
  for (size_t s = slots; s < 512; s++) boot_l1[s] = 0; /* unmapped */

  boot_l0[0] = table_desc(boot_l1);

  bootmmu_enable();

  /* From here on RAM is Normal and atomics are legal — arm the serial
   * SMP lock (unlocked plain MMIO until now, safe only single-core). */
  serial_smp_arm();
  serial_puts("mmu: boot identity map on — RAM top ");
  serial_print_hex(ram_top);
  serial_puts(", ");
  serial_print_dec(nbanks);
  serial_puts(" bank(s) Normal WB, rest Device\n");
}

void bootmmu_ap_enable(void) {
  bootmmu_enable(); /* same tables, this core's own sysregs */
  serial_puts("mmu: ap identity map on\n");
}
