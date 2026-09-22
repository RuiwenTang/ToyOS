/*
 * memmap.h — physical memory map: DTB discovery + normalization
 *
 * The aarch64 replacement for Limine's memory map: banks come from the
 * DTB /memory node(s), reservations from the FDT /memreserve/ block plus
 * the kernel image and the DTB blob itself. memmap_build turns the two
 * lists into the normalized usable-region list the pmm consumes (the
 * direct analogue of a limine_memmap_response with USABLE entries).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_MEMMAP_H
#define TOYOS_KERNEL_MEMMAP_H

#include <toyos/kernel/types.h>

/* Must match pmm.h PAGE_SIZE; defined independently so the host-testable
 * build logic has no pmm dependency. */
#define MEMMAP_PAGE_SIZE 4096

#define MEMMAP_MAX_BANKS 16     /* /memory reg tuples across all nodes */
#define MEMMAP_MAX_RESERVED 32  /* /memreserve/ + kernel + DTB entries */
#define MEMMAP_MAX_REGIONS 32   /* normalized usable output */

/* memmap_build error return: output capacity exceeded (or inputs nested
 * too deeply) — a boot-fatal condition, not a partial result. */
#define MEMMAP_BUILD_OVERFLOW ((size_t)-1)

typedef struct mem_region {
  uint64_t base;
  uint64_t len;
} mem_region_t;

/*
 * memmap_build - banks minus reservations, normalized
 *
 * @banks:     RAM banks (any order, may overlap/abut)
 * @nbanks:    entry count (<= MEMMAP_MAX_BANKS)
 * @reserved:  regions to subtract (kernel image, DTB, /memreserve/…)
 * @nreserved: entry count
 * @out:       output region array, page-aligned + sorted by base
 * @cap:       entries in @out
 *
 * Pure function, host-testable: clamps banks to whole pages (base up, end
 * down — partial pages are never usable) and reservations the other way
 * (base down, end up — a partially-touched page stays reserved), sorts and
 * merges banks, subtracts, and emits sorted non-overlapping usable
 * regions. Returns the region count, or MEMMAP_BUILD_OVERFLOW if @cap
 * cannot hold the result.
 */
size_t memmap_build(const mem_region_t* banks, size_t nbanks,
                    const mem_region_t* reserved, size_t nreserved,
                    mem_region_t* out, size_t cap);

/* --- kernel-side init (serial + DTB; not built on the host) --- */

/*
 * memmap_init - discover the memory map from the boot DTB
 *
 * @dtb:     boot DTB blob pointer (x0 handoff)
 * @kstart:  kernel image load start (linker symbol)
 * @kend:    kernel image end (linker symbol; .bss + boot stacks included)
 * @dtb_end: DTB blob end (dtb + fdt_valid size)
 *
 * Walks root children with device_type = "memory" (multi-bank), collects
 * /memreserve/ entries, adds the image/DTB reservations, builds and stores
 * the normalized map. Returns 0, or -1 (no banks / storage overflow).
 */
int memmap_init(const void* dtb, uint64_t kstart, uint64_t kend,
                uint64_t dtb_end);

/* Raw discovered banks (bootmmu maps these; sorted + merged). */
const mem_region_t* memmap_banks(size_t* n);

/* Normalized usable regions (pmm consumes these). */
const mem_region_t* memmap_usable(size_t* n);

/* Total usable bytes (post-subtraction). */
uint64_t memmap_total_usable(void);

/* Highest RAM byte + 1 across all banks — bootmmu coverage input. */
uint64_t memmap_ram_top(void);

#endif /* TOYOS_KERNEL_MEMMAP_H */
