/*
 * bitmap.h — Bitmap utility for the physical memory manager
 *
 * Provides bit-level set/clear/test and search operations on a
 * byte-aligned bitmap buffer.
 *
 * Ported from ToyOS64 lib/libkernel (BSD-3 relicense, sole author).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_BITMAP_H
#define TOYOS_KERNEL_BITMAP_H

#include <toyos/kernel/types.h>

/*
 * bitmap_set - Set a bit (mark as used)
 *
 * @bitmap:   Byte-aligned bitmap buffer
 * @bit:      Bit index to set
 */
void bitmap_set(uint8_t* bitmap, size_t bit);

/*
 * bitmap_clear - Clear a bit (mark as free)
 *
 * @bitmap:   Byte-aligned bitmap buffer
 * @bit:      Bit index to clear
 */
void bitmap_clear(uint8_t* bitmap, size_t bit);

/*
 * bitmap_test - Test whether a bit is set
 *
 * @bitmap:   Byte-aligned bitmap buffer
 * @bit:      Bit index to test
 *
 * Returns true if the bit is set, false otherwise.
 */
bool bitmap_test(const uint8_t* bitmap, size_t bit);

/*
 * bitmap_find_first_clear - Find the first clear bit
 *
 * @bitmap:   Byte-aligned bitmap buffer
 * @num_bits: Total number of bits to search
 *
 * Returns the index of the first clear bit, or -1 if none found.
 */
ssize_t bitmap_find_first_clear(const uint8_t* bitmap, size_t num_bits);

/*
 * bitmap_find_clear_region - Find a contiguous region of clear bits
 *
 * @bitmap:   Byte-aligned bitmap buffer
 * @num_bits: Total number of bits to search
 * @count:    Number of contiguous clear bits to find
 *
 * Returns the starting index of the first suitable region, or -1 if none
 * found.
 */
ssize_t bitmap_find_clear_region(const uint8_t* bitmap, size_t num_bits,
                                 size_t count);

#endif /* TOYOS_KERNEL_BITMAP_H */
