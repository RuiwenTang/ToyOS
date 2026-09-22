/*
 * bitmap.c — Bitmap utility implementation
 *
 * Bit-level operations on a byte-aligned bitmap buffer.
 * Used by the Physical Memory Manager (PMM) for page tracking.
 *
 * Ported from ToyOS64 lib/libkernel (BSD-3 relicense, sole author).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/bitmap.h>
#include <toyos/kernel/types.h>

void bitmap_set(uint8_t* bitmap, size_t bit) {
  bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

void bitmap_clear(uint8_t* bitmap, size_t bit) {
  bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

bool bitmap_test(const uint8_t* bitmap, size_t bit) {
  return (bitmap[bit / 8] & (1u << (bit % 8))) != 0;
}

ssize_t bitmap_find_first_clear(const uint8_t* bitmap, size_t num_bits) {
  for (size_t i = 0; i < num_bits; i++) {
    if (!bitmap_test(bitmap, i)) return (ssize_t)i;
  }
  return -1;
}

ssize_t bitmap_find_clear_region(const uint8_t* bitmap, size_t num_bits,
                                 size_t count) {
  if (count == 0) return -1;

  size_t run = 0;
  for (size_t i = 0; i < num_bits; i++) {
    if (!bitmap_test(bitmap, i)) {
      run++;
      if (run == count) return (ssize_t)(i - count + 1);
    } else {
      run = 0;
    }
  }
  return -1;
}
