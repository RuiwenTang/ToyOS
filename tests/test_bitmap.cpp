/*
 * test_bitmap.cpp — Host-side unit tests for the bitmap utility
 *
 * Uses Google Test. The bitmap implementation (kernel/mm/bitmap.c) is
 * compiled as C and linked here via extern "C".
 *
 * Ported from ToyOS64 tests (BSD-3 relicense, sole author).
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include <toyos/kernel/bitmap.h>
}

/* --- bitmap_set / bitmap_test --- */

TEST(BitmapSetTest, SetSingleBit) {
  uint8_t buf[4] = {0};

  bitmap_set(buf, 0);
  EXPECT_TRUE(bitmap_test(buf, 0));
  EXPECT_EQ(buf[0], 1);

  /* all other bits should still be 0 */
  for (size_t i = 1; i < 32; i++) EXPECT_FALSE(bitmap_test(buf, i));
}

TEST(BitmapSetTest, SetMultipleBits) {
  uint8_t buf[4] = {0};

  bitmap_set(buf, 0);
  bitmap_set(buf, 7);
  bitmap_set(buf, 8);
  bitmap_set(buf, 31);

  EXPECT_TRUE(bitmap_test(buf, 0));
  EXPECT_TRUE(bitmap_test(buf, 7));
  EXPECT_TRUE(bitmap_test(buf, 8));
  EXPECT_TRUE(bitmap_test(buf, 31));

  /* byte 0: bit 0 + bit 7 = 0x81 */
  EXPECT_EQ(buf[0], 0x81);
  /* byte 1: bit 8 = bit 0 of byte 1 = 0x01 */
  EXPECT_EQ(buf[1], 0x01);
  /* byte 3: bit 31 = bit 7 of byte 3 = 0x80 */
  EXPECT_EQ(buf[3], 0x80);
}

TEST(BitmapSetTest, SetCrossByteBoundary) {
  uint8_t buf[2] = {0};

  bitmap_set(buf, 7);
  bitmap_set(buf, 8);
  bitmap_set(buf, 9);

  EXPECT_TRUE(bitmap_test(buf, 7));
  EXPECT_TRUE(bitmap_test(buf, 8));
  EXPECT_TRUE(bitmap_test(buf, 9));
  EXPECT_EQ(buf[0], 0x80);
  EXPECT_EQ(buf[1], 0x03);
}

TEST(BitmapSetTest, SetAllBitsInByte) {
  uint8_t buf[1] = {0};

  for (size_t i = 0; i < 8; i++) bitmap_set(buf, i);

  EXPECT_EQ(buf[0], 0xFF);
  for (size_t i = 0; i < 8; i++) EXPECT_TRUE(bitmap_test(buf, i));
}

/* --- bitmap_clear --- */

TEST(BitmapClearTest, ClearSetBit) {
  uint8_t buf[1] = {0xFF};

  EXPECT_TRUE(bitmap_test(buf, 3));
  bitmap_clear(buf, 3);
  EXPECT_FALSE(bitmap_test(buf, 3));
  EXPECT_EQ(buf[0], 0xF7);
}

TEST(BitmapClearTest, SetThenClearCycle) {
  uint8_t buf[2] = {0};

  bitmap_set(buf, 10);
  EXPECT_TRUE(bitmap_test(buf, 10));
  bitmap_clear(buf, 10);
  EXPECT_FALSE(bitmap_test(buf, 10));
  EXPECT_EQ(buf[0], 0);
  EXPECT_EQ(buf[1], 0);
}

TEST(BitmapClearTest, ClearAllBitsInByte) {
  uint8_t buf[1] = {0xFF};

  for (size_t i = 0; i < 8; i++) bitmap_clear(buf, i);

  EXPECT_EQ(buf[0], 0x00);
}

TEST(BitmapClearTest, ClearAlreadyClearBit_IsNoOp) {
  uint8_t buf[1] = {0x00};

  bitmap_clear(buf, 3);
  EXPECT_EQ(buf[0], 0x00);
  EXPECT_FALSE(bitmap_test(buf, 3));
}

/* --- bitmap_find_first_clear --- */

TEST(BitmapFindFirstClearTest, AllClear) {
  uint8_t buf[4] = {0};

  EXPECT_EQ(bitmap_find_first_clear(buf, 32), 0);
}

TEST(BitmapFindFirstClearTest, AllSet) {
  uint8_t buf[4];
  std::memset(buf, 0xFF, sizeof(buf));

  EXPECT_EQ(bitmap_find_first_clear(buf, 32), -1);
}

TEST(BitmapFindFirstClearTest, FirstBitSet) {
  uint8_t buf[4] = {0};
  bitmap_set(buf, 0);

  EXPECT_EQ(bitmap_find_first_clear(buf, 32), 1);
}

TEST(BitmapFindFirstClearTest, HoleInMiddle) {
  uint8_t buf[4];
  std::memset(buf, 0xFF, sizeof(buf));

  /* clear bit 15 */
  bitmap_clear(buf, 15);

  EXPECT_EQ(bitmap_find_first_clear(buf, 32), 15);
}

TEST(BitmapFindFirstClearTest, OnlyLastBitClear) {
  uint8_t buf[4];
  std::memset(buf, 0xFF, sizeof(buf));

  bitmap_clear(buf, 31);

  EXPECT_EQ(bitmap_find_first_clear(buf, 32), 31);
}

TEST(BitmapFindFirstClearTest, SingleByteBitmap) {
  uint8_t buf[1] = {0xFE}; /* only bit 0 is clear */

  EXPECT_EQ(bitmap_find_first_clear(buf, 8), 0);
}

TEST(BitmapFindFirstClearTest, ZeroBits) {
  uint8_t buf[1] = {0};

  EXPECT_EQ(bitmap_find_first_clear(buf, 0), -1);
}

/* --- bitmap_find_clear_region --- */

TEST(BitmapFindClearRegionTest, RequestZero) {
  uint8_t buf[4] = {0};

  EXPECT_EQ(bitmap_find_clear_region(buf, 32, 0), -1);
}

TEST(BitmapFindClearRegionTest, RequestOne) {
  uint8_t buf[4] = {0};

  EXPECT_EQ(bitmap_find_clear_region(buf, 32, 1), 0);
}

TEST(BitmapFindClearRegionTest, AllClearFindFour) {
  uint8_t buf[4] = {0};

  EXPECT_EQ(bitmap_find_clear_region(buf, 32, 4), 0);
}

TEST(BitmapFindClearRegionTest, ExactFit) {
  /* 8 bits: 0-3 clear, 4-7 set */
  uint8_t buf[1] = {0xF0};

  EXPECT_EQ(bitmap_find_clear_region(buf, 8, 4), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, 8, 5), -1);
}

TEST(BitmapFindClearRegionTest, Fragmented) {
  /* pattern: 0 0 1 0 0 1 0 0 — two 2-bit clear regions at 0 and 3, one at 6 */
  uint8_t buf[1] = {0x24}; /* 00100100 */

  EXPECT_EQ(bitmap_find_clear_region(buf, 8, 2), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, 8, 3), -1);
}

TEST(BitmapFindClearRegionTest, CrossByteBoundary) {
  uint8_t buf[2] = {0};
  /* set everything except bits 7 and 8 */
  std::memset(buf, 0xFF, sizeof(buf));
  bitmap_clear(buf, 7);
  bitmap_clear(buf, 8);

  EXPECT_EQ(bitmap_find_clear_region(buf, 16, 2), 7);
  EXPECT_EQ(bitmap_find_clear_region(buf, 16, 3), -1);
}

TEST(BitmapFindClearRegionTest, AllSet) {
  uint8_t buf[4];
  std::memset(buf, 0xFF, sizeof(buf));

  EXPECT_EQ(bitmap_find_clear_region(buf, 32, 1), -1);
  EXPECT_EQ(bitmap_find_clear_region(buf, 32, 4), -1);
}

TEST(BitmapFindClearRegionTest, RegionAtEnd) {
  /* 16 bits: only bits 12-15 are clear = byte1 high nibble clear = 0x0F */
  uint8_t buf[2] = {0xFF, 0x0F};

  EXPECT_EQ(bitmap_find_clear_region(buf, 16, 4), 12);
  EXPECT_EQ(bitmap_find_clear_region(buf, 16, 5), -1);
}

TEST(BitmapFindClearRegionTest, LargeContiguousRegion) {
  uint8_t buf[16] = {0}; /* 128 bits, all clear */

  EXPECT_EQ(bitmap_find_clear_region(buf, 128, 128), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, 128, 129), -1);
}

/* --- boundary / stress tests --- */

TEST(BitmapBoundaryTest, LargeBitmap) {
  /* simulate a 4096-byte bitmap (like pmm for ~128 MiB) */
  const size_t size = 4096;
  const size_t num_bits = size * 8;
  auto* buf = new uint8_t[size];

  /* all set */
  std::memset(buf, 0xFF, size);
  EXPECT_EQ(bitmap_find_first_clear(buf, num_bits), -1);

  /* clear one bit near the end */
  bitmap_clear(buf, num_bits - 1);
  EXPECT_EQ(bitmap_find_first_clear(buf, num_bits),
            static_cast<ssize_t>(num_bits - 1));

  /* all clear */
  std::memset(buf, 0, size);
  EXPECT_EQ(bitmap_find_first_clear(buf, num_bits), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, num_bits, num_bits), 0);

  /* set every other bit */
  for (size_t i = 0; i < num_bits; i += 2) bitmap_set(buf, i);
  EXPECT_EQ(bitmap_find_clear_region(buf, num_bits, 2), -1);
  EXPECT_EQ(bitmap_find_first_clear(buf, num_bits), 1);

  delete[] buf;
}

TEST(BitmapBoundaryTest, SingleBitBitmap) {
  uint8_t buf[1] = {0};

  EXPECT_EQ(bitmap_find_first_clear(buf, 1), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, 1, 1), 0);
  EXPECT_EQ(bitmap_find_clear_region(buf, 1, 2), -1);

  bitmap_set(buf, 0);
  EXPECT_EQ(bitmap_find_first_clear(buf, 1), -1);
  EXPECT_TRUE(bitmap_test(buf, 0));

  bitmap_clear(buf, 0);
  EXPECT_EQ(bitmap_find_first_clear(buf, 1), 0);
  EXPECT_FALSE(bitmap_test(buf, 0));
}
