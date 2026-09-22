/*
 * memmap.c — physical memory map: DTB discovery + normalization
 *
 * Split in two layers on the heap.c pattern: memmap_build is pure logic
 * (host-testable), memmap_init is the kernel-side DTB/serial glue.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/memmap.h>
#include <toyos/kernel/types.h>

#ifdef __TOYOS_KERNEL__
#include <toyos/arch/aarch64/cpu.h>
#include <toyos/kernel/fdt.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/string.h>
#endif

static uint64_t align_up64(uint64_t val, uint64_t align) {
  return (val + align - 1) & ~(align - 1);
}

static uint64_t align_down64(uint64_t val, uint64_t align) {
  return val & ~(align - 1);
}

/* --- build: banks - reservations -> sorted page-aligned regions --- */

static void sort_regions(mem_region_t* r, size_t n) {
  /* insertion sort — n is tiny (<= 32) and this runs once at boot */
  for (size_t i = 1; i < n; i++) {
    mem_region_t key = r[i];
    size_t j = i;
    while (j > 0 && r[j - 1].base > key.base) {
      r[j] = r[j - 1];
      j--;
    }
    r[j] = key;
  }
}

size_t memmap_build(const mem_region_t* banks, size_t nbanks,
                    const mem_region_t* reserved, size_t nreserved,
                    mem_region_t* out, size_t cap) {
  mem_region_t merged[MEMMAP_MAX_BANKS];
  size_t nmerged = 0;
  size_t nout = 0;

  /* 1. Clamp banks to whole pages and drop empties: base up, end down —
   * a partial page at either edge is never handed out. */
  for (size_t i = 0; i < nbanks; i++) {
    uint64_t base = align_up64(banks[i].base, MEMMAP_PAGE_SIZE);
    uint64_t end = align_down64(banks[i].base + banks[i].len,
                                MEMMAP_PAGE_SIZE);
    if (end <= base) continue;
    if (nmerged == MEMMAP_MAX_BANKS) return MEMMAP_BUILD_OVERFLOW;
    merged[nmerged].base = base;
    merged[nmerged].len = end - base;
    nmerged++;
  }

  /* 2. Sort by base, then fuse overlaps/abutments in place (a DTB should
   * never overlap banks, but normalizing costs nothing and simplifies the
   * subtraction). Empty input stays empty — the write cursor math below
   * assumes at least one bank. */
  sort_regions(merged, nmerged);
  if (nmerged > 0) {
    size_t w = 0; /* write cursor */
    for (size_t i = 1; i < nmerged; i++) {
      uint64_t wend = merged[w].base + merged[w].len;
      if (merged[i].base <= wend) { /* overlap or abutting */
        uint64_t iend = merged[i].base + merged[i].len;
        if (iend > wend) merged[w].len = iend - merged[w].base;
      } else {
        merged[++w] = merged[i];
      }
    }
    nmerged = w + 1;
  }

  /* 3. Subtract each reservation from every bank. Reservations clamp the
   * conservative way — base down, end up — so a reservation touching part
   * of a page keeps the whole page out of the usable set. Everything is
   * page-aligned by now, so the fragments come out aligned. */
  for (size_t b = 0; b < nmerged; b++) {
    /* fragments of this bank, built up as reservations punch holes */
    mem_region_t frags[MEMMAP_MAX_RESERVED + 1];
    size_t nfrags = 1;
    frags[0].base = merged[b].base;
    frags[0].len = merged[b].len;

    for (size_t r = 0; r < nreserved && nfrags > 0; r++) {
      uint64_t rbase = align_down64(reserved[r].base, MEMMAP_PAGE_SIZE);
      uint64_t rend = align_up64(reserved[r].base + reserved[r].len,
                                 MEMMAP_PAGE_SIZE);
      if (rend <= rbase) continue; /* zero-length (or wrapped) reservation */

      for (size_t f = 0; f < nfrags; f++) {
        uint64_t fend = frags[f].base + frags[f].len;
        if (rbase >= fend || rend <= frags[f].base) continue; /* no overlap */

        /* Overlap: this fragment splits into at most a below-part and an
         * above-part. Build them, then rewrite the slot with below (if
         * any) and append above (if any). */
        mem_region_t below = {frags[f].base,
                              rbase > frags[f].base
                                  ? rbase - frags[f].base
                                  : 0};
        mem_region_t above = {rend, rend < fend ? fend - rend : 0};

        if (below.len > 0 && above.len > 0) {
          if (nfrags == MEMMAP_MAX_RESERVED + 1)
            return MEMMAP_BUILD_OVERFLOW;
          frags[f] = below;
          frags[nfrags++] = above;
        } else if (below.len > 0) {
          frags[f] = below;
        } else if (above.len > 0) {
          frags[f] = above;
        } else {
          /* fully consumed: swap-remove */
          frags[f] = frags[nfrags - 1];
          nfrags--;
          f--; /* re-examine the swapped-in fragment against this rsv */
        }
      }
    }

    /* Fragment order is scrambled by the split-then-append subtraction
     * (an "above" fragment lands after untouched later fragments); the
     * API promises sorted output, and banks are already sorted, so a
     * per-bank sort makes the whole output sorted. */
    sort_regions(frags, nfrags);
    for (size_t f = 0; f < nfrags; f++) {
      if (nout == cap) return MEMMAP_BUILD_OVERFLOW;
      out[nout++] = frags[f];
    }
  }

  return nout;
}

/* --- kernel side: DTB discovery + stored map --- */

#ifdef __TOYOS_KERNEL__

static mem_region_t k_banks[MEMMAP_MAX_BANKS];
static size_t k_nbanks;
static mem_region_t k_reserved[MEMMAP_MAX_RESERVED];
static size_t k_nreserved;
static mem_region_t k_usable[MEMMAP_MAX_REGIONS];
static size_t k_nusable;

/* Big-endian cell accumulation: 1-2 cells (more is not a thing for RAM
 * on our targets; extra high cells would already exceed 48-bit PA). */
static uint64_t cells_to_u64(const void* data, uint32_t ncells) {
  const uint8_t* p = data;
  uint64_t v = 0;
  for (uint32_t i = 0; i < ncells; i++)
    v = (v << 32) | fdt_cell32(p + i * 4);
  return v;
}

/* device_type = "memory" walk over the root's children. Node names embed
 * unit addresses that vary per board (memory@40000000), so a path lookup
 * is wrong — this is the canonical discovery (devicetree spec §3.4.3). */
static int collect_banks(const void* dtb) {
  fdt_node_t root = fdt_find_node(dtb, "/");
  if (root < 0) return -1;

  uint32_t ac = 2, sc = 2; /* arm64 default; root always carries these */
  fdt_get_prop_u32(dtb, root, "#address-cells", &ac);
  fdt_get_prop_u32(dtb, root, "#size-cells", &sc);
  if (ac > 2 || sc > 2 || ac == 0 || sc == 0) {
    serial_puts("memmap: absurd /memory cell counts (ac=");
    serial_print_dec(ac);
    serial_puts(", sc=");
    serial_print_dec(sc);
    serial_puts(")\n");
    return -1;
  }

  for (fdt_node_t n = fdt_child_first(dtb, root); n >= 0;
       n = fdt_child_next(dtb, n)) {
    const void* dtype;
    int dlen = fdt_get_prop(dtb, n, "device_type", &dtype);
    if (dlen < 0 || strcmp(dtype, "memory") != 0) continue;

    const void* reg;
    int rlen = fdt_get_prop(dtb, n, "reg", &reg);
    if (rlen < 0) continue;

    uint32_t stride = ac + sc;
    for (uint32_t off = 0; off + stride <= (uint32_t)rlen / 4;
         off += stride) {
      uint64_t addr = cells_to_u64((const uint8_t*)reg + off * 4, ac);
      uint64_t size =
          cells_to_u64((const uint8_t*)reg + (off + ac) * 4, sc);
      if (size == 0) continue;
      if (k_nbanks == MEMMAP_MAX_BANKS) return -1;
      k_banks[k_nbanks].base = addr;
      k_banks[k_nbanks].len = size;
      k_nbanks++;
      serial_puts("memmap: bank [");
      serial_print_hex(addr);
      serial_puts(", ");
      serial_print_hex(addr + size);
      serial_puts(")\n");
    }
  }
  return k_nbanks > 0 ? 0 : -1;
}

int memmap_init(const void* dtb, uint64_t kstart, uint64_t kend,
                uint64_t dtb_end) {
  k_nbanks = 0;
  k_nreserved = 0;
  k_nusable = 0;

  if (collect_banks(dtb) != 0) {
    serial_puts("memmap: no /memory banks in DTB, halting\n");
    cpu_halt();
  }

  /* Reservations: /memreserve/ block, the kernel image (covers .bss and
   * the boot stacks — they live in the image region), and the DTB blob
   * itself (both loaders place it inside RAM). */
  {
    int idx = 0;
    uint64_t a, s;
    while ((idx = fdt_mem_rsv(dtb, idx, &a, &s)) >= 0) {
      if (s == 0) continue;
      if (k_nreserved == MEMMAP_MAX_RESERVED) {
        serial_puts("memmap: too many reservations, halting\n");
        cpu_halt();
      }
      k_reserved[k_nreserved].base = a;
      k_reserved[k_nreserved].len = s;
      k_nreserved++;
      serial_puts("memmap: /memreserve/ [");
      serial_print_hex(a);
      serial_puts(", ");
      serial_print_hex(a + s);
      serial_puts(")\n");
    }
  }
  k_reserved[k_nreserved].base = kstart;
  k_reserved[k_nreserved].len = kend - kstart;
  k_nreserved++;
  k_reserved[k_nreserved].base = (uint64_t)(uintptr_t)dtb;
  k_reserved[k_nreserved].len = dtb_end - (uint64_t)(uintptr_t)dtb;
  k_nreserved++;
  serial_puts("memmap: kernel image [");
  serial_print_hex(kstart);
  serial_puts(", ");
  serial_print_hex(kend);
  serial_puts("), dtb [");
  serial_print_hex((uint64_t)(uintptr_t)dtb);
  serial_puts(", ");
  serial_print_hex(dtb_end);
  serial_puts(")\n");

  k_nusable = memmap_build(k_banks, k_nbanks, k_reserved, k_nreserved,
                           k_usable, MEMMAP_MAX_REGIONS);
  if (k_nusable == MEMMAP_BUILD_OVERFLOW) {
    serial_puts("memmap: region overflow, halting\n");
    cpu_halt();
  }

  for (size_t i = 0; i < k_nusable; i++) {
    serial_puts("memmap: usable [");
    serial_print_hex(k_usable[i].base);
    serial_puts(", ");
    serial_print_hex(k_usable[i].base + k_usable[i].len);
    serial_puts(")\n");
  }
  serial_puts("memmap: ");
  serial_print_dec(memmap_total_usable() / (1024 * 1024));
  serial_puts(" MiB usable across ");
  serial_print_dec(k_nusable);
  serial_puts(" region(s)\n");
  return 0;
}

const mem_region_t* memmap_banks(size_t* n) {
  *n = k_nbanks;
  return k_banks;
}

const mem_region_t* memmap_usable(size_t* n) {
  *n = k_nusable;
  return k_usable;
}

uint64_t memmap_total_usable(void) {
  uint64_t total = 0;
  for (size_t i = 0; i < k_nusable; i++) total += k_usable[i].len;
  return total;
}

uint64_t memmap_ram_top(void) {
  uint64_t top = 0;
  for (size_t i = 0; i < k_nbanks; i++) {
    uint64_t end = k_banks[i].base + k_banks[i].len;
    if (end > top) top = end;
  }
  return top;
}

#endif /* __TOYOS_KERNEL__ */
