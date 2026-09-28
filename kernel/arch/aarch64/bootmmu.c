/*
 * bootmmu.c — boot-time MMU bring-up: identity trampoline + kernel tree
 *
 * Why the MMU must go on early at all: with it off, all memory is Device
 * type, and Device memory does not support atomic or exclusive operations
 * — LDAXR/STXR/LDADD raise DFSC 0x35 (alignment, HVF) or 0x61 (unsupported
 * atomic hardware update, TCG). The scheduler is built on atomics; first
 * hit was heap_lock, the very first spin_lock_irqsave.
 *
 * R2.2 shape — two static trees, enabled together (layout in kva.h):
 *
 *   TTBR0 (identity trampoline): the R1 map verbatim — [0, coverage)
 *     with RAM banks Normal WB and the rest Device. Covers everything the
 *     still-physical execution window touches (image + boot stacks + MMIO
 *     for the pre-flip serial/GIC). Dropped per core after the whole
 *     system runs at the link VA and device access has flipped to the
 *     direct map.
 *
 *   TTBR1 (kernel tree, never switched): L0 slot 256 = the direct map
 *     (512 GiB PA window, RAM Normal WB PXN / rest Device — every MMIO PA
 *     on both targets); slot 511 = kernel scratch: the image at its link
 *     VA in 4 KiB pages (arm64's text_offset 0x80000 is NOT 2 MiB-aligned,
 *     so blocks are out), L1 #508 pre-linked to an empty kstack L2 (the
 *     paging walker fills L3s from pmm), L1 #509 (kernel heap) left for
 *     R2.4. The root is static on purpose: TTBR1 never switches, so the
 *     root needs no pmm phase-in; only growing intermediates are dynamic.
 *
 * Both trees' tables live in .bss, and bootmmu_init runs at the physical
 * base — every C pointer IS its physical address there (ADRP resolves
 * against the running PC), which is exactly what TTBR registers want.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/bootmmu.h>
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/arch/aarch64/kva.h>
#include <toyos/arch/aarch64/sysreg.h>
#include <toyos/kernel/memmap.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/types.h>

/* Descriptor types, bits[1:0] (same encoding as Linux's PMD_TYPE_*):
 * 0b01 = block/section (L1/L2), 0b11 = table (L0-L2) or page (L3).
 * Attribute indices match the MAIR_EL1 programming below. SH=11
 * inner-shareable, AF=1 (access flag — never fault), AP=00 EL1 RW;
 * PXN set on data regions (direct map), clear on the image (kernel text). */
#define DESC_BLOCK (1u << 0)
#define DESC_TABLE (3u << 0)

#define ATTR_DEVICE 0x0 /* MAIR attr 0: Device-nGnRE */
#define ATTR_NORMAL 0x1 /* MAIR attr 1: Normal WB RW */

#define DESC_PXN (1ull << 53)

#define PAGE_DESC(pa, attr)                                        \
  ((((uint64_t)(pa)) & 0x0000fffffffff000ull) /* PA bits[47:12] */ \
   | DESC_TABLE                               /* L3 = page      */ \
   | (((uint64_t)(attr)) << 2)                /* AttrIndx       */ \
   | (1u << 10)                               /* AF             */ \
   | (3u << 8))                               /* SH inner       */

#define BLOCK_DESC(pa_base, attr)                                           \
  ((((uint64_t)(pa_base)) & 0x0000ffffffe00000ull) /* bits[47:21], 2 MiB */ \
   | DESC_BLOCK                                    /* bits[1:0]        */   \
   | (((uint64_t)(attr)) << 2)                     /* AttrIndx         */   \
   | (1u << 10)                                    /* AF               */   \
   | (3u << 8))                                    /* SH               */

#define L1_SLOT_BYTES (1ull * 1024 * 1024 * 1024)
#define BLOCK_BYTES (2u * 1024 * 1024)

/* One L2 table per 1 GiB L1 slot in coverage. The 4 GiB floor needs 4;
 * 8 GiB of RK3568 DRAM (the ceiling target) reaches slot 9 → 10. The pool
 * covers RAM up to ~13 GiB; more than that is a compile-time bump. */
#define BOOT_L2_POOL 16

/* --- identity trampoline (TTBR0): the proven R1 map --- */

static uint64_t boot_l0[512] __attribute__((aligned(4096)));
static uint64_t boot_l1[512] __attribute__((aligned(4096)));
static uint64_t boot_l2_pool[BOOT_L2_POOL][512] __attribute__((aligned(4096)));

/* TTBR0 target after the drop: all-invalid (the "no user tree" state). */
static uint64_t empty_l0[512] __attribute__((aligned(4096)));

/* --- kernel tree (TTBR1, layout in kva.h) --- */

static uint64_t kern_l0[512] __attribute__((aligned(4096)));
static uint64_t kern_dm_l1[512] __attribute__((aligned(4096)));
static uint64_t kern_dm_l2[BOOT_L2_POOL][512] __attribute__((aligned(4096)));
static uint64_t kern_l1_511[512] __attribute__((aligned(4096)));
static uint64_t kern_l2_image[512] __attribute__((aligned(4096)));
/* Image pages: 4 KiB granularity (text_offset is not 2 MiB-aligned). Four
 * L3s cover 8 MiB of image — bss + boot stacks included, far beyond the
 * current image; bootmmu_init checks the fit at runtime. */
#define KERN_IMAGE_L3_POOL 4
static uint64_t kern_image_l3[KERN_IMAGE_L3_POOL][512]
    __attribute__((aligned(4096)));
/* kstack region L2: all-invalid at boot, filled by the paging walker
 * (kernel/arch/aarch64/paging.c) with pmm-backed L3s. */
uint64_t kern_l2_kstack[512] __attribute__((aligned(4096)));

/* Device VA window offset (see mmio.h); .bss zero = identity trampoline. */
uint64_t mmio_va_offset;

static uint64_t table_desc(uint64_t* table) {
  return (uint64_t)(uintptr_t)table | DESC_TABLE;
}

/* The paging walker's entry into this tree (its dynamic L3s link in like
 * any other level). */
uint64_t* paging_kernel_root(void);

uint64_t* paging_kernel_root(void) { return kern_l0; }

/* Is [pa, pa+2 MiB) fully inside a discovered RAM bank? (Banks are sorted
 * and merged by memmap, but a plain scan is fine — this runs once.) */
static int block_is_ram(const mem_region_t* banks, size_t nbanks, uint64_t pa) {
  for (size_t i = 0; i < nbanks; i++) {
    if (pa >= banks[i].base && pa + BLOCK_BYTES <= banks[i].base + banks[i].len)
      return 1;
  }
  return 0;
}

/* Fill one L2 table for a 1 GiB coverage slot: RAM blocks Normal (PXN
 * unless @exec), rest Device-nGnRE. */
static void fill_l2(uint64_t* l2, uint64_t slot_base, const mem_region_t* banks,
                    size_t nbanks, int exec) {
  for (uint32_t b = 0; b < 512; b++) {
    uint64_t pa = slot_base + (uint64_t)b * BLOCK_BYTES;
    l2[b] = block_is_ram(banks, nbanks, pa)
                ? (BLOCK_DESC(pa, ATTR_NORMAL) | (exec ? 0 : DESC_PXN))
                : BLOCK_DESC(pa, ATTR_DEVICE);
  }
}

/* Program MAIR/TCR/both TTBRs from the (already built) static tables and
 * turn the MMU + caches on. Shared by the BSP (after building) and every
 * AP (tables shared read-only after boot) — SCTLR/TCR/TTBR are per-core
 * registers, so each core must run this itself. */
static void bootmmu_enable(void) {
  /* MAIR: attr 0 = Device-nGnRE, attr 1 = Normal WB RW-cacheable. */
  const uint64_t mair = ((uint64_t)0x04 << 0) | ((uint64_t)0xff << 8);
  sysreg_write(MAIR_EL1, mair);

  /* TCR_EL1: 48-bit VA both halves (T0SZ=T1SZ=16), 4 KiB granule on both,
   * 40-bit PA (IPS=2 — QEMU virt and the RK3568's 36-bit), inner-shareable
   * WB walks on both. TG0 lives at [15:14] (00 = 4 KiB) but TG1 at
   * [31:30] with an INVERTED encoding (0b10 = 4 KiB) — writing TG1 into
   * TG0's slot silently selects a 16 KiB walk for TTBR0 and every 4 KiB
   * table miswalks (the R2.2 boot hang, IFSC 0x05 at level 2). */
  const uint64_t tcr = (16u << 0)      /* T0SZ */
                       | (3u << 12)    /* SH0 inner-shareable */
                       | (1u << 10)    /* ORGN0 write-back */
                       | (1u << 8)     /* IRGN0 write-back */
                       | (16u << 16)   /* T1SZ = 48-bit VA */
                       | (2u << 30)    /* TG1 = 4 KiB (inverted encoding) */
                       | (3u << 28)    /* SH1 inner-shareable */
                       | (1u << 26)    /* ORGN1 write-back */
                       | (1u << 24)    /* IRGN1 write-back */
                       | (2ull << 32); /* IPS = 40-bit */
  sysreg_write(TCR_EL1, tcr);
  sysreg_write(TTBR0_EL1, (uint64_t)(uintptr_t)boot_l0);
  sysreg_write(TTBR1_EL1, (uint64_t)(uintptr_t)kern_l0);

  barrier_dsb_sy(); /* tables + sysregs land before the MMU reads them */
  barrier_isb();

  uint64_t sctlr = sysreg_read(SCTLR_EL1);
  sctlr |= (1u << 0)     /* M: MMU on */
           | (1u << 2)   /* C: data cache */
           | (1u << 12); /* I: instruction cache */
  sysreg_write(SCTLR_EL1, sctlr);
  barrier_isb();
}

/* [0, max(4 GiB, align_up(ram_top, 1 GiB))): the 4 GiB floor keeps the
 * RK3568's above-DRAM peripherals mapped (they live below 4 GiB); QEMU
 * virt keeps all MMIO in slot 0 either way. */
static uint64_t coverage_end(uint64_t ram_top) {
  uint64_t floor = 4ull * 1024 * 1024 * 1024;
  uint64_t ram_aligned =
      (ram_top + L1_SLOT_BYTES - 1) & ~(uint64_t)(L1_SLOT_BYTES - 1);
  return ram_aligned > floor ? ram_aligned : floor;
}

/* Image at its link VA, 4 KiB pages. The image's PA comes from
 * KERNEL_IMAGE_BASE − kimage_offset (see the kva.h warning for why not
 * kva_to_pa(&symbol)); its VA pages then run from KERNEL_IMAGE_BASE in
 * lockstep (+kimage_offset), so the L2/L3 indices come from the VA offset
 * within the 1 GiB region, NOT from where the PA sits in its own blocks
 * (PA 0x40080000 is 0x80000 into a 2 MiB block — indexing by PA put the
 * image 1 GiB up and the jump faulted silently). Holes inside a partially
 * covered 2 MiB block stay invalid — faults instead of aliasing RAM. */
static void build_image_map(void) {
  extern const char __kernel_start[], __kernel_end[];
  uint64_t image_pa = KERNEL_IMAGE_BASE - kimage_offset;
  uint64_t image_bytes =
      (uint64_t)(uintptr_t)__kernel_end - (uint64_t)(uintptr_t)__kernel_start;
  uint64_t off_end = (image_bytes + 0xfff) & ~(uint64_t)0xfff;
  uint32_t l3_used = 0;

  for (uint32_t b = 0; b < 512; b++) {
    uint64_t block_off = (uint64_t)b * BLOCK_BYTES;
    if (block_off >= off_end) {
      kern_l2_image[b] = 0; /* whole 2 MiB beyond the image */
      continue;
    }
    if (l3_used == KERN_IMAGE_L3_POOL) {
      serial_puts("mmu: image exceeds L3 pool — bump KERN_IMAGE_L3_POOL\n");
      cpu_halt();
    }
    uint64_t* l3 = kern_image_l3[l3_used++];
    for (uint32_t p = 0; p < 512; p++) {
      uint64_t off = block_off + (uint64_t)p * 4096;
      l3[p] = (off < off_end) ? PAGE_DESC(image_pa + off, ATTR_NORMAL) : 0;
    }
    kern_l2_image[b] = table_desc(l3);
  }
}

void bootmmu_init(const mem_region_t* banks, size_t nbanks) {
  uint64_t ram_top = memmap_ram_top();
  uint64_t cover_end = coverage_end(ram_top);
  size_t slots = (size_t)(cover_end / L1_SLOT_BYTES);

  if (slots > BOOT_L2_POOL) {
    serial_puts("mmu: coverage needs ");
    serial_print_dec(slots);
    serial_puts(" L2 tables, pool is ");
    serial_print_dec(BOOT_L2_POOL);
    serial_puts(" — bump BOOT_L2_POOL\n");
    cpu_halt();
  }

  /* Trampoline (TTBR0): the proven R1 map — RAM Normal (executable, the
   * low world's code runs here), rest Device. */
  for (size_t s = 0; s < slots; s++) {
    fill_l2(boot_l2_pool[s], (uint64_t)s * L1_SLOT_BYTES, banks, nbanks, 1);
    boot_l1[s] = table_desc(boot_l2_pool[s]);
  }
  for (size_t s = slots; s < 512; s++) boot_l1[s] = 0;
  boot_l0[0] = table_desc(boot_l1);

  /* Kernel tree: direct map (L0 slot 256), data-only (PXN — nothing
   * executes from the direct map). */
  for (size_t s = 0; s < slots; s++) {
    fill_l2(kern_dm_l2[s], (uint64_t)s * L1_SLOT_BYTES, banks, nbanks, 0);
    kern_dm_l1[s] = table_desc(kern_dm_l2[s]);
  }
  for (size_t s = slots; s < 512; s++) kern_dm_l1[s] = 0;
  kern_l0[(KERNEL_DM_BASE >> 39) & 0x1FF] = table_desc(kern_dm_l1);

  /* Kernel tree: slot 511 = image (4K pages) + kstack L2 (empty); the
   * heap slot stays invalid until R2.4. */
  build_image_map();
  kern_l1_511[(KERNEL_IMAGE_BASE >> 30) & 0x1FF] = table_desc(kern_l2_image);
  kern_l1_511[(KERNEL_KSTACK_BASE >> 30) & 0x1FF] = table_desc(kern_l2_kstack);
  kern_l0[511] = table_desc(kern_l1_511);

  bootmmu_enable();

  /* From here on RAM is Normal and atomics are legal — arm the serial
   * SMP lock (unlocked plain MMIO until now, safe only single-core). */
  serial_smp_arm();
  serial_puts(
      "mmu: trampoline (TTBR0) + kernel tree (TTBR1) on, executing "
      "at physical until the jump\n");
}

void bootmmu_ap_enable(void) {
  bootmmu_enable(); /* same shared tables, this core's own sysregs */
  serial_puts("mmu: ap both trees on (low)\n");
}

void bootmmu_drop_trampoline(void) {
  sysreg_write(TTBR0_EL1, (uint64_t)kva_to_pa((uintptr_t)empty_l0));
  /* Full local flush (covers both TTBR0/TTBR1 entries — the kernel-tree
   * ones are simply refetched; this runs once per core). */
  __asm__ __volatile__(
      "dsb ish\n"
      "tlbi vmalle1\n"
      "dsb ish\n"
      "isb" ::
          : "memory");
}
