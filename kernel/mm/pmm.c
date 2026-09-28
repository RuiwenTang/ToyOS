/*
 * pmm.c — Physical Memory Manager
 *
 * Bitmap-based allocator tracking free physical pages at 4 KiB granularity.
 * Consumes the normalized memmap usable regions and dynamically places the
 * bitmap (then the per-frame refcount table) in the first suitable region,
 * accessed through pmm_phys_to_virt.
 *
 * Ported from ToyOS64 kernel/mm/pmm.c (BSD-3 relicense, sole author).
 * Substitutions beyond includes, all mechanical:
 *   - limine_memmap_response -> const mem_region_t* usable regions
 *     (built by memmap_init from the DTB; kernel/DTB/memreserve pages are
 *     already excluded there, which is what Limine's USABLE entries did)
 *   - HHDM offset -> pmm_phys_to_virt/pmm_virt_to_phys (identity in R2.1,
 *     direct-map offset in R2.2)
 *   - "cli; hlt" halt loops -> cpu_halt()
 *   - pmm_reclaim_bootloader dropped: Limine-specific, no analogue in the
 *     DTB boot path (bootloader memory arrives as /memreserve/ entries)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/arch/aarch64/cpu.h>
#include <toyos/kernel/bitmap.h>
#include <toyos/kernel/memmap.h>
#include <toyos/kernel/pmm.h>
#include <toyos/kernel/refcount.h>
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
#include <toyos/kernel/string.h>
#include <toyos/kernel/types.h>

/* --- Internal state (all BSS-allocated) --- */

static uint8_t* pmm_bitmap;    /* bitmap buffer (via pmm_phys_to_virt) */
static size_t pmm_bitmap_size; /* number of bytes in the bitmap */
static size_t pmm_max_page;    /* highest page index (max_phys / PAGE_SIZE) */
static size_t pmm_total;       /* total physical pages managed */
static size_t pmm_free_count;  /* currently free pages */
static struct refcount_t* page_refcount; /* per-frame refcount (CoW fork) */
static spinlock_t pmm_lock; /* serialises alloc/free bitmap mutation */

/* --- Helpers --- */

static uint64_t align_up(uint64_t val, uint64_t align) {
  return (val + align - 1) & ~(align - 1);
}

static uint64_t align_down(uint64_t val, uint64_t align) {
  return val & ~(align - 1);
}

/* --- Initialization --- */

void pmm_init(const mem_region_t* usable, size_t n) {
  if (n == 0) {
    serial_puts("[PMM] ERROR: no usable regions\n");
    cpu_halt();
  }

  serial_puts("[PMM] Initializing...\n");

  spinlock_init(&pmm_lock);

  /* Step 1: highest physical address among the usable regions — the
   * bitmap only needs to reach that far (nothing above is ever handed
   * out). */
  uint64_t max_phys = 0;
  for (size_t i = 0; i < n; i++) {
    uint64_t end = usable[i].base + usable[i].len;
    if (end > max_phys) max_phys = end;
  }

  /* Step 2: bitmap dimensions */
  pmm_max_page = (size_t)((max_phys + PAGE_SIZE - 1) / PAGE_SIZE);
  pmm_bitmap_size = (pmm_max_page + 7) / 8;

  serial_puts("[PMM] Max physical address: ");
  serial_print_hex(max_phys);
  serial_puts(" (");
  serial_print_dec(max_phys / (1024 * 1024));
  serial_puts(" MiB)\n");

  serial_puts("[PMM] Bitmap size: ");
  serial_print_dec(pmm_bitmap_size);
  serial_puts(" bytes (");
  serial_print_dec(pmm_bitmap_size / 1024);
  serial_puts(" KiB), tracking ");
  serial_print_dec(pmm_max_page);
  serial_puts(" pages\n");

  /* Step 3: find a region large enough for the bitmap; place it at the
   * region's start. */
  pmm_bitmap = NULL;
  uint64_t bitmap_phys = 0;
  for (size_t i = 0; i < n; i++) {
    if (usable[i].len >= pmm_bitmap_size) {
      bitmap_phys = usable[i].base;
      pmm_bitmap = (uint8_t*)pmm_phys_to_virt(bitmap_phys);
      break;
    }
  }

  if (!pmm_bitmap) {
    serial_puts("[PMM] ERROR: no usable region large enough for bitmap (");
    serial_print_dec(pmm_bitmap_size);
    serial_puts(" bytes needed)\n");
    cpu_halt();
  }

  /* Step 4: all pages marked used (0xFF) */
  memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
  pmm_total = 0;
  pmm_free_count = 0;

  /* Step 5: mark usable pages free. Regions are page-aligned already
   * (memmap_build), but keep the clamp — it is what upstream does and it
   * is cheap insurance. */
  for (size_t i = 0; i < n; i++) {
    uint64_t base = align_up(usable[i].base, PAGE_SIZE);
    uint64_t end = align_down(usable[i].base + usable[i].len, PAGE_SIZE);

    for (uint64_t addr = base; addr < end; addr += PAGE_SIZE) {
      size_t idx = (size_t)(addr / PAGE_SIZE);
      if (idx < pmm_max_page) {
        bitmap_clear(pmm_bitmap, idx);
        pmm_free_count++;
      }
    }
    pmm_total = pmm_free_count; /* total = all free pages (usable only) */
  }

  /* Step 6: mark the bitmap's own pages used (it sits inside a region we
   * just marked free). */
  uint64_t bitmap_end_phys = align_up(bitmap_phys + pmm_bitmap_size, PAGE_SIZE);
  for (uint64_t addr = align_down(bitmap_phys, PAGE_SIZE);
       addr < bitmap_end_phys; addr += PAGE_SIZE) {
    size_t idx = (size_t)(addr / PAGE_SIZE);
    if (idx < pmm_max_page && !bitmap_test(pmm_bitmap, idx)) {
      bitmap_set(pmm_bitmap, idx);
      pmm_free_count--;
    }
  }

  /* Step 7: summary */
  serial_puts("[PMM] Bitmap placed at physical ");
  serial_print_hex(bitmap_phys);
  serial_puts("\n");
  serial_puts("[PMM] Total usable pages: ");
  serial_print_dec(pmm_total);
  serial_puts(" (");
  serial_print_dec(pmm_total * PAGE_SIZE / 1024);
  serial_puts(" KiB)\n");
  serial_puts("[PMM] Free pages: ");
  serial_print_dec(pmm_free_count);
  serial_puts(" (");
  serial_print_dec(pmm_free_count * PAGE_SIZE / 1024);
  serial_puts(" KiB)\n");

  /* Step 8: per-frame reference-count table (backs CoW fork). One
   * refcount_t per managed frame, placed like the bitmap: first region
   * that fits; if the only fit is the bitmap's own region, right after
   * the bitmap's pages. Its pages are marked used so pmm_alloc never
   * hands them out. */
  size_t refcount_bytes = pmm_max_page * sizeof(struct refcount_t);
  page_refcount = NULL;
  uint64_t rc_phys = 0;
  for (size_t i = 0; i < n; i++) {
    if (bitmap_phys >= usable[i].base &&
        bitmap_phys < usable[i].base + usable[i].len) {
      /* Bitmap's region: place the table right after the bitmap. */
      uint64_t after_bitmap =
          align_up(bitmap_phys + pmm_bitmap_size, PAGE_SIZE);
      if (after_bitmap + refcount_bytes <= usable[i].base + usable[i].len) {
        rc_phys = after_bitmap;
        page_refcount = (struct refcount_t*)pmm_phys_to_virt(rc_phys);
        break;
      }
      continue; /* bitmap region cannot also hold the refcount table */
    }

    if (usable[i].len >= refcount_bytes) {
      rc_phys = usable[i].base;
      page_refcount = (struct refcount_t*)pmm_phys_to_virt(rc_phys);
      break;
    }
  }
  if (!page_refcount) {
    serial_puts(
        "[PMM] ERROR: no usable region large enough for refcount table (");
    serial_print_dec(refcount_bytes);
    serial_puts(" bytes needed)\n");
    cpu_halt();
  }
  memset(page_refcount, 0, refcount_bytes);

  /* Mark the refcount table's own pages used. */
  uint64_t rc_end = align_up(rc_phys + refcount_bytes, PAGE_SIZE);
  for (uint64_t addr = align_down(rc_phys, PAGE_SIZE); addr < rc_end;
       addr += PAGE_SIZE) {
    size_t idx = (size_t)(addr / PAGE_SIZE);
    if (idx < pmm_max_page && !bitmap_test(pmm_bitmap, idx)) {
      bitmap_set(pmm_bitmap, idx);
      pmm_free_count--;
    }
  }

  serial_puts("[PMM] Refcount table placed at physical ");
  serial_print_hex(rc_phys);
  serial_puts(" (");
  serial_print_dec(refcount_bytes / 1024);
  serial_puts(" KiB)\n");
  serial_puts("[PMM] Initialized successfully\n");
}

/* --- Allocation --- */

uintptr_t pmm_alloc(void) {
  spinlock_acquire(&pmm_lock);
  ssize_t idx = bitmap_find_first_clear(pmm_bitmap, pmm_max_page);
  if (idx < 0) {
    spinlock_release(&pmm_lock);
    return 0; /* out of memory */
  }

  bitmap_set(pmm_bitmap, (size_t)idx);
  pmm_free_count--;
  refcount_init(&page_refcount[(size_t)idx],
                1); /* freshly allocated frame is exclusively owned */
  spinlock_release(&pmm_lock);

  return (uintptr_t)idx << PAGE_SHIFT;
}

void pmm_free(uintptr_t phys) {
  if (phys == 0 || (phys & (PAGE_SIZE - 1)) != 0) {
    serial_puts("[PMM] ERROR: pmm_free called with invalid address ");
    serial_print_hex(phys);
    serial_puts("\n");
    cpu_halt();
  }

  size_t idx = (size_t)(phys >> PAGE_SHIFT);
  if (idx >= pmm_max_page) {
    serial_puts("[PMM] ERROR: pmm_free index out of range\n");
    cpu_halt();
  }

  spinlock_acquire(&pmm_lock);
  /* double-free detection */
  if (!bitmap_test(pmm_bitmap, idx)) {
    spinlock_release(&pmm_lock);
    serial_puts("[PMM] WARNING: double free detected at physical ");
    serial_print_hex(phys);
    serial_puts("\n");
    return;
  }

  bitmap_clear(pmm_bitmap, idx);
  pmm_free_count++;
  spinlock_release(&pmm_lock);
}

bool pmm_is_managed(uintptr_t phys) {
  return (phys >> PAGE_SHIFT) < pmm_max_page;
}

uintptr_t pmm_alloc_pages(size_t count) {
  if (count == 0) return 0;
  if (count == 1) return pmm_alloc();

  spinlock_acquire(&pmm_lock);
  ssize_t idx = bitmap_find_clear_region(pmm_bitmap, pmm_max_page, count);
  if (idx < 0) {
    spinlock_release(&pmm_lock);
    return 0; /* no contiguous region found */
  }

  for (size_t i = 0; i < count; i++) bitmap_set(pmm_bitmap, (size_t)idx + i);

  pmm_free_count -= count;
  spinlock_release(&pmm_lock);

  return (uintptr_t)idx << PAGE_SHIFT;
}

void pmm_free_pages(uintptr_t phys, size_t count) {
  for (size_t i = 0; i < count; i++) pmm_free(phys + (uintptr_t)i * PAGE_SIZE);
}

/* --- Diagnostics --- */

size_t pmm_total_pages(void) { return pmm_total; }

size_t pmm_free_page_count(void) { return pmm_free_count; }

/* --- Reference counting (Copy-on-Write fork) --- */

void pmm_refcount_inc(uintptr_t phys) {
  size_t idx = (size_t)(phys >> PAGE_SHIFT);
  if (idx >= pmm_max_page) return;
  refcount_get(&page_refcount[idx]);
}

uint32_t pmm_refcount_dec(uintptr_t phys) {
  size_t idx = (size_t)(phys >> PAGE_SHIFT);
  if (idx >= pmm_max_page) return 0;
  if (refcount_put(&page_refcount[idx]))
    return 0; /* hit 0 -> caller frees the frame */
  /* Not zero (or underflow, which refcount_put already undid). */
  int cur = refcount_read(&page_refcount[idx]);
  if (cur <= 0) {
    /* Underflow: more puts than gets on this frame. refcount_put undid
     * the dec; return nonzero so the caller does NOT free a frame still
     * referenced elsewhere (the aliasing that was the -smp 4 crash). */
    serial_puts("[PMM] refcount underflow phys=");
    serial_print_hex(phys);
    serial_puts(" (dec ignored, frame not freed)\n");
    return 1;
  }
  return (uint32_t)cur;
}

uint32_t pmm_refcount_get(uintptr_t phys) {
  size_t idx = (size_t)(phys >> PAGE_SHIFT);
  if (idx >= pmm_max_page) return 0;
  return (uint32_t)refcount_read(&page_refcount[idx]);
}
