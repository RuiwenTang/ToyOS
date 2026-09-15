/*
 * heap.c — Kernel heap allocator for ToyOS64
 *
 * Linked-list free-block allocator with boundary tags for coalescing.
 * Provider-agnostic: uses heap_provider_t callbacks for page allocation,
 * enabling host-side unit testing.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/heap.h>
#include <toyos/kernel/string.h>
#include <toyos/kernel/types.h>

/*
 * The heap free list is a global structure. On SMP, concurrent kmalloc/kfree
 * from two cores corrupts it (Phase 8 will refine this; for now a single
 * spinlock serialises all heap mutations). Host unit tests are single-
 * threaded, so the lock is kernel-only.
 */
#ifdef __TOYOS_KERNEL__
#include <toyos/arch/aarch64/cpu.h> /* cpu_halt (the only arch touch in this file) */
#include <toyos/kernel/serial.h>
#include <toyos/kernel/spinlock.h>
static spinlock_t heap_lock;
#define HEAP_LOCK() spinlock_acquire(&heap_lock)
#define HEAP_UNLOCK() spinlock_release(&heap_lock)
#define HEAP_LOCK_INIT() spinlock_init(&heap_lock)
#else
#include <stdlib.h>
#define HEAP_LOCK() ((void)0)
#define HEAP_UNLOCK() ((void)0)
#define HEAP_LOCK_INIT() ((void)0)
#endif

/* --- Constants --- */

#define HEAP_ALIGN 16
#define HEADER_SIZE sizeof(block_header_t)
#define FOOTER_SIZE sizeof(block_footer_t)
#define METADATA_SIZE (HEADER_SIZE + FOOTER_SIZE) /* 32 bytes */
#define MIN_PAYLOAD 16 /* must hold 2 pointers when free */
#define MIN_BLOCK_SIZE (METADATA_SIZE + MIN_PAYLOAD) /* 48 bytes */

#define HEAP_MAGIC_ALLOC 0xA110CA7Eu /* "ALLOCATE" */
#define HEAP_MAGIC_FREE 0xF2EEB10Cu  /* "FREE BLOCK" */

/* KASAN-style overflow detection (diagnostic for -smp 4 memory corruption).
 * A tail redzone is appended to every allocated payload and stamped with
 * HEAP_MAGIC_REDZONE; heap_check verifies it on every touch, so a write past
 * the end of any allocation (e.g. overflowing into a neighbour TCB's
 * kernel_rsp, which later surfaces as the random "current = garbage" schedule
 * crash) is caught at the next kmalloc/kfree instead of far from the crime. */
#define HEAP_REDZONE 16
#define HEAP_MAGIC_REDZONE 0x00BADADEu

/* KASAN UAF detection: kfree poisons the payload past the free-list ptrs;
 * kmalloc verifies it on reuse — a non-poison byte means a freed block was
 * written after free (use-after-free), which is invisible to redzone/header
 * checks and surfaces later as random control-flow corruption. */
#define HEAP_POISON_BYTE 0xDDu

#define BLOCK_USED 0
#define BLOCK_FREE 1

/* --- Block header (16 bytes, at start of every block) --- */

typedef struct block_header {
  size_t size;    /* usable payload size (excluding header + footer) */
  uint32_t flags; /* BLOCK_FREE or BLOCK_USED */
  uint32_t magic; /* HEAP_MAGIC_ALLOC or HEAP_MAGIC_FREE */
} block_header_t;

/* --- Block footer / boundary tag (16 bytes, at end of every block) --- */

typedef struct block_footer {
  size_t size;    /* must match header.size */
  uint32_t magic; /* must match header.magic */
  uint32_t _pad;
} block_footer_t;

/* --- Free list pointers (overlaid on payload of free blocks) --- */

typedef struct free_ptrs {
  block_header_t* next_free;
  block_header_t* prev_free;
} free_ptrs_t;

/* --- Heap state --- */

typedef struct heap_state {
  block_header_t* free_list; /* head of doubly-linked free list */
  uintptr_t heap_start;      /* virtual address of heap region */
  size_t heap_size;          /* total bytes committed */
  size_t heap_used;          /* bytes currently allocated (payload) */
  size_t alloc_count;        /* number of active allocations */
  const heap_provider_t* provider;
  void* provider_ctx;
} heap_state_t;

static heap_state_t g_heap;

/* --- Helpers --- */

static size_t align_up(size_t val, size_t align) {
  return (val + align - 1) & ~(align - 1);
}

/* Get the footer of a block */
static block_footer_t* get_footer(block_header_t* block) {
  return (block_footer_t*)((uint8_t*)block + HEADER_SIZE + block->size);
}

/* Get the header of the previous block from its footer (for backward
 * coalescing). footer = prev_block + HEADER_SIZE + prev_block->size So
 * prev_block = footer - footer->size - HEADER_SIZE */
static block_header_t* prev_block_from_footer(block_footer_t* footer) {
  return (block_header_t*)((uint8_t*)footer - footer->size - HEADER_SIZE);
}

/* Check if an address is within the heap region */
static int is_heap_ptr(void* ptr) {
  uintptr_t addr = (uintptr_t)ptr;
  return addr >= g_heap.heap_start &&
         addr < g_heap.heap_start + g_heap.heap_size;
}

/*
 * heap_panic - Halt with a dump when heap metadata is found corrupted.
 *
 * This boundary-tag allocator is fragile under overflow/UAF: if a block's
 * header or footer (size/magic) is clobbered, kfree's coalescing chases bogus
 * neighbour pointers and turns a local corruption into global free-list
 * breakage — surfacing later as the multi-point "random field overwritten"
 * SMP crashes in schedule(). Dumping the first corrupted block at the point
 * it's touched gives a crime scene instead of a mystery #PF. Kernel: serial +
 * halt; host unit tests: abort.
 */
#ifdef __TOYOS_KERNEL__
static void heap_panic(const char* where, block_header_t* b) {
  serial_puts("[heap] CORRUPT @");
  serial_puts(where);
  serial_puts(" block=0x");
  serial_print_hex((uint64_t)(uintptr_t)b);
  if (b && is_heap_ptr(b)) {
    serial_puts(" magic=0x");
    serial_print_hex(b->magic);
    serial_puts(" flags=");
    serial_print_dec(b->flags);
    serial_puts(" size=0x");
    serial_print_hex(b->size);
    if (b->size > 0 && b->size <= g_heap.heap_size) {
      block_footer_t* f =
          (block_footer_t*)((uint8_t*)b + HEADER_SIZE + b->size);
      serial_puts(" fmagic=0x");
      serial_print_hex(f->magic);
      serial_puts(" fsize=0x");
      serial_print_hex(f->size);
    }
  }
  serial_puts(" used=0x");
  serial_print_hex(g_heap.heap_used);
  serial_puts(" allocs=");
  serial_print_dec(g_heap.alloc_count);
  serial_puts("\n");
  cpu_halt();
}
#else
static void heap_panic(const char* where, block_header_t* b) {
  (void)where;
  (void)b;
  abort();
}
#endif

/*
 * heap_check - Verify a block's header/footer consistency. Called on every
 * block touched by kmalloc (free-list scan) and kfree (the block being freed
 * plus its coalescing neighbours), so the first corrupted block is caught at
 * the point of use rather than after the damage has spread.
 */
static void heap_check(block_header_t* b, uint32_t expect_magic,
                       const char* where) {
  if (!is_heap_ptr(b)) heap_panic(where, b);
  if (b->magic != expect_magic) heap_panic(where, b);
  if (b->size == 0 || b->size > g_heap.heap_size) heap_panic(where, b);
  uintptr_t footer_addr = (uintptr_t)b + HEADER_SIZE + b->size;
  if (footer_addr + FOOTER_SIZE > g_heap.heap_start + g_heap.heap_size)
    heap_panic(where, b);
  block_footer_t* f = (block_footer_t*)footer_addr;
  if (f->magic != b->magic || f->size != b->size) heap_panic(where, b);

  /* Tail redzone (KASAN): only USED blocks carry one. A clobbered word here
   * means some allocation wrote past its end into a neighbour. */
  if (expect_magic == HEAP_MAGIC_ALLOC && b->size >= HEAP_REDZONE) {
    uint32_t* rz =
        (uint32_t*)((uint8_t*)b + HEADER_SIZE + b->size - HEAP_REDZONE);
    for (int i = 0; i < HEAP_REDZONE / 4; i++) {
      if (rz[i] != HEAP_MAGIC_REDZONE) heap_panic("redzone-overflow", b);
    }
  }
}

/*
 * heap_check_live_alloc - LOCK-FREE envelope check of a known-live USED block.
 * See declaration in heap.h. Mirrors heap_check()'s USED-block checks but
 * returns 0 on corruption instead of halting, so the scheduler diagnostic can
 * print the victim thread's context before stopping. Safe to call without
 * heap_lock because USED-block metadata is only mutated at alloc/free of that
 * exact block, never by a concurrent operation on a different block.
 */
int heap_check_live_alloc(void* user_ptr) {
  block_header_t* b = (block_header_t*)((uint8_t*)user_ptr - HEADER_SIZE);

  if (!is_heap_ptr(b)) return 0;
  if (b->magic != HEAP_MAGIC_ALLOC) return 0;
  if (b->size == 0 || b->size > g_heap.heap_size) return 0;

  block_footer_t* f = get_footer(b);
  if (f->magic != b->magic || f->size != b->size) return 0;

  /* Tail redzone (high end = stack_top canary). A clobbered word means a
   * higher-address neighbour overflowed downward into this block's top. */
  if (b->size >= HEAP_REDZONE) {
    uint32_t* rz =
        (uint32_t*)((uint8_t*)b + HEADER_SIZE + b->size - HEAP_REDZONE);
    for (int i = 0; i < HEAP_REDZONE / 4; i++) {
      if (rz[i] != HEAP_MAGIC_REDZONE) return 0;
    }
  }
  return 1;
}

/* Get free-list pointers from a free block's payload */
static free_ptrs_t* get_free_ptrs(block_header_t* block) {
  return (free_ptrs_t*)((uint8_t*)block + HEADER_SIZE);
}

/* --- Free list management --- */

static void free_list_insert(block_header_t* block) {
  /* KASAN: poison the payload past the free-list ptrs ([16..size]) so a
   * write-after-free is detectable when this block is next kmalloc'd. This
   * is the single chokepoint — every free block (kfree final, kmalloc split
   * remainder, heap_grow) goes through here, so none is left unpoisoned. */
  if (block->size > 16) {
    uint8_t* p = (uint8_t*)block + HEADER_SIZE + 16;
    size_t n = block->size - 16;
    for (size_t i = 0; i < n; i++) p[i] = HEAP_POISON_BYTE;
  }

  free_ptrs_t* fp = get_free_ptrs(block);
  fp->prev_free = NULL;
  fp->next_free = g_heap.free_list;
  if (g_heap.free_list) {
    free_ptrs_t* head_fp = get_free_ptrs(g_heap.free_list);
    head_fp->prev_free = block;
  }
  g_heap.free_list = block;
}

static void free_list_remove(block_header_t* block) {
  free_ptrs_t* fp = get_free_ptrs(block);

  if (fp->prev_free) {
    free_ptrs_t* prev_fp = get_free_ptrs(fp->prev_free);
    prev_fp->next_free = fp->next_free;
  } else {
    g_heap.free_list = fp->next_free;
  }

  if (fp->next_free) {
    free_ptrs_t* next_fp = get_free_ptrs(fp->next_free);
    next_fp->prev_free = fp->prev_free;
  }

  fp->next_free = NULL;
  fp->prev_free = NULL;
}

/* --- Block operations --- */

/*
 * Set header and footer for a block.
 * All-in-one to ensure they stay consistent.
 */
static void block_set_meta(block_header_t* block, size_t payload_size,
                           uint32_t flags, uint32_t magic) {
  block->size = payload_size;
  block->flags = flags;
  block->magic = magic;

  block_footer_t* footer = get_footer(block);
  footer->size = payload_size;
  footer->magic = magic;
  footer->_pad = 0;
}

/* --- Heap growth --- */

/*
 * heap_grow - Request more pages from the provider and create a free block.
 *
 * Returns 0 on success, -1 on failure.
 */
static int heap_grow(size_t min_payload_needed) {
  size_t total_needed =
      METADATA_SIZE + align_up(min_payload_needed, HEAP_ALIGN);
  size_t pages_needed = (total_needed + HEAP_PAGE_SIZE - 1) / HEAP_PAGE_SIZE;

  void* new_pages =
      g_heap.provider->alloc_pages(g_heap.provider_ctx, pages_needed);
  if (!new_pages) return -1;

  size_t new_size = pages_needed * HEAP_PAGE_SIZE;
  uintptr_t new_addr = (uintptr_t)new_pages;

  /*
   * If new pages are contiguous with the current heap end,
   * and the last block is free, extend it instead of creating a new block.
   */
  if (g_heap.heap_size > 0 &&
      new_addr == g_heap.heap_start + g_heap.heap_size &&
      g_heap.free_list != NULL) {
    /* Find the last block in the heap */
    block_header_t* last = g_heap.free_list;
    uintptr_t last_end =
        (uintptr_t)last + HEADER_SIZE + last->size + FOOTER_SIZE;

    if (last_end == new_addr && last->magic == HEAP_MAGIC_FREE) {
      /* Extend the last free block */
      free_list_remove(last);
      size_t new_payload = last->size + new_size;
      block_set_meta(last, new_payload, BLOCK_FREE, HEAP_MAGIC_FREE);
      free_list_insert(last);
      g_heap.heap_size += new_size;
      return 0;
    }
  }

  /* Create a new free block spanning the entire new region */
  block_header_t* block = (block_header_t*)new_pages;
  size_t payload = new_size - METADATA_SIZE;
  block_set_meta(block, payload, BLOCK_FREE, HEAP_MAGIC_FREE);
  free_list_insert(block);

  /* Update heap state */
  if (g_heap.heap_size == 0) g_heap.heap_start = new_addr;
  g_heap.heap_size += new_size;

  return 0;
}

/* --- Public API --- */

void heap_init(const heap_provider_t* provider, void* provider_ctx,
               uintptr_t start_addr) {
  g_heap.free_list = NULL;
  g_heap.heap_start = start_addr;
  g_heap.heap_size = 0;
  g_heap.heap_used = 0;
  g_heap.alloc_count = 0;
  g_heap.provider = provider;
  g_heap.provider_ctx = provider_ctx;
  HEAP_LOCK_INIT();
}

void* kmalloc(size_t size) {
  if (size == 0) return NULL;

  /* Round up to alignment, enforce minimum payload, add tail redzone */
  size_t needed = align_up(size, HEAP_ALIGN);
  if (needed < MIN_PAYLOAD) needed = MIN_PAYLOAD;
  needed += HEAP_REDZONE; /* KASAN tail redzone */

  HEAP_LOCK();

  /* First-fit search on free list */
  block_header_t* block = g_heap.free_list;
  while (block) {
    heap_check(block, HEAP_MAGIC_FREE, "kmalloc-scan");
    if (block->size >= needed) {
      /* KASAN UAF check: payload past the free-list ptrs ([16..size])
       * must still carry the kfree poison. A non-poison byte here means
       * this freed block was written after free. Skipped for freshly-
       * grown blocks (never poisoned; they never come through the scan
       * — grow only runs when the scan finds nothing). */
      if (block->size > 16) {
        uint8_t* p = (uint8_t*)block + HEADER_SIZE + 16;
        size_t chk = block->size - 16;
        if (chk > 64) chk = 64;
        for (size_t i = 0; i < chk; i++) {
          if (p[i] != HEAP_POISON_BYTE) heap_panic("use-after-free", block);
        }
      }
      break;
    }
    free_ptrs_t* fp = get_free_ptrs(block);
    block = fp->next_free;
  }

  if (!block) {
    /* No suitable free block — grow the heap */
    if (heap_grow(needed) < 0) {
      HEAP_UNLOCK();
      return NULL;
    }

    /* Retry: the new free block should be at the head of the list */
    block = g_heap.free_list;
    if (!block || block->size < needed) {
      HEAP_UNLOCK();
      return NULL; /* should not happen */
    }
    heap_check(block, HEAP_MAGIC_FREE, "kmalloc-grow");
  }

  /* Try to split if there's enough leftover */
  size_t remaining = block->size - needed;
  if (remaining >= MIN_BLOCK_SIZE) {
    /* Remove from free list, split, then re-insert remainder */
    free_list_remove(block);

    /* Set up the allocated portion */
    block_set_meta(block, needed, BLOCK_USED, HEAP_MAGIC_ALLOC);

    /* Create remainder block */
    block_header_t* remainder =
        (block_header_t*)((uint8_t*)block + HEADER_SIZE + needed + FOOTER_SIZE);
    size_t rem_payload = remaining - METADATA_SIZE;
    block_set_meta(remainder, rem_payload, BLOCK_FREE, HEAP_MAGIC_FREE);
    free_list_insert(remainder);
  } else {
    /* Use the whole block */
    free_list_remove(block);
    block_set_meta(block, block->size, BLOCK_USED, HEAP_MAGIC_ALLOC);
  }

  /* KASAN: stamp tail redzone at the end of the payload */
  uint32_t* rz =
      (uint32_t*)((uint8_t*)block + HEADER_SIZE + block->size - HEAP_REDZONE);
  for (int i = 0; i < HEAP_REDZONE / 4; i++) rz[i] = HEAP_MAGIC_REDZONE;

  g_heap.heap_used += block->size;
  g_heap.alloc_count++;

  HEAP_UNLOCK();
  return (void*)((uint8_t*)block + HEADER_SIZE);
}

void kfree(void* ptr) {
  if (!ptr) return;

  block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);

  /* Sanity check */
  if (block->magic != HEAP_MAGIC_ALLOC) {
    /* Invalid free — in kernel this would panic; in tests, just return */
    return;
  }

  HEAP_LOCK();

  /* Catch a block whose header/footer was clobbered (overflow from a
   * neighbour, or UAF reuse) BEFORE coalescing chases the bad footer and
   * spreads the damage through the free list. */
  heap_check(block, HEAP_MAGIC_ALLOC, "kfree");

  g_heap.heap_used -= block->size;
  g_heap.alloc_count--;

  /* Mark as free */
  block_set_meta(block, block->size, BLOCK_FREE, HEAP_MAGIC_FREE);

  /* --- Forward coalescing: merge with next block --- */
  block_footer_t* footer = get_footer(block);
  block_header_t* next_block =
      (block_header_t*)((uint8_t*)footer + FOOTER_SIZE);

  if (is_heap_ptr(next_block) && next_block->magic == HEAP_MAGIC_FREE) {
    heap_check(next_block, HEAP_MAGIC_FREE, "kfree-fwd");
    free_list_remove(next_block);
    /* Merge: block absorbs next_block */
    size_t new_size =
        block->size + FOOTER_SIZE + HEADER_SIZE + next_block->size;
    block_set_meta(block, new_size, BLOCK_FREE, HEAP_MAGIC_FREE);
  }

  /* --- Backward coalescing: merge with previous block --- */
  if ((uintptr_t)block > g_heap.heap_start) {
    block_footer_t* prev_footer =
        (block_footer_t*)((uint8_t*)block - FOOTER_SIZE);

    if (prev_footer->magic == HEAP_MAGIC_FREE) {
      block_header_t* prev_block = prev_block_from_footer(prev_footer);

      if (is_heap_ptr(prev_block) && prev_block->magic == HEAP_MAGIC_FREE) {
        heap_check(prev_block, HEAP_MAGIC_FREE, "kfree-bwd");
        free_list_remove(prev_block);
        /* Merge: prev_block absorbs block */
        size_t new_size =
            prev_block->size + FOOTER_SIZE + HEADER_SIZE + block->size;
        block_set_meta(prev_block, new_size, BLOCK_FREE, HEAP_MAGIC_FREE);
        block = prev_block;
      }
    }
  }

  /* Insert the (possibly merged) block into the free list */
  free_list_insert(block);

  HEAP_UNLOCK();
}

void kfree_sized(void* ptr, size_t size) {
  (void)size;
  kfree(ptr);
}

void* kcalloc(size_t nmemb, size_t size) {
  /* Overflow check */
  if (nmemb != 0 && size != 0) {
    size_t total = nmemb * size;
    if (total / nmemb != size) return NULL;
  } else {
    return NULL;
  }

  size_t total = nmemb * size;
  void* ptr = kmalloc(total);
  if (ptr) memset(ptr, 0, total);
  return ptr;
}

void* krealloc(void* ptr, size_t size) {
  if (!ptr) return kmalloc(size);

  if (size == 0) {
    kfree(ptr);
    return NULL;
  }

  block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);

  if (block->magic != HEAP_MAGIC_ALLOC) return NULL;

  /* If current block is large enough, return same pointer */
  if (block->size - HEAP_REDZONE >= size) return ptr;

  /* Allocate new block, copy data, free old */
  void* new_ptr = kmalloc(size);
  if (!new_ptr) return NULL;

  size_t usable = block->size - HEAP_REDZONE;
  size_t copy_size = usable < size ? usable : size;
  memcpy(new_ptr, ptr, copy_size);
  kfree(ptr);

  return new_ptr;
}

void heap_dump_stats(void) {
  /*
   * Minimal stats output — no serial dependency.
   * Kernel wrapper can read these and print via serial.
   *
   * For now this is a no-op placeholder that can be enhanced
   * when a kernel logging infrastructure is available.
   */
  (void)g_heap;
}
