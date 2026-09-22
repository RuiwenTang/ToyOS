/*
 * test_memmap.cpp — Host-side unit tests for memmap_build
 *
 * The normalization layer (banks minus reservations -> sorted,
 * page-aligned, non-overlapping usable regions) is where every
 * discovery bug class lives: alignment clamping in both directions,
 * multi-bank input, reservations that split / swallow / straddle banks,
 * abutting-fragment merging. The DTB glue (memmap_init) is kernel-only —
 * smoke covers it end to end.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gtest/gtest.h>

#include <vector>

extern "C" {
#include <toyos/kernel/memmap.h>
}

namespace {

constexpr uint64_t kPage = MEMMAP_PAGE_SIZE;
constexpr uint64_t kMiB = 1024 * 1024;

struct Region {
  uint64_t base, len;
};

/* Run memmap_build with vector-backed storage; returns the regions (empty
 * on overflow, which tests assert separately via the raw call). */
std::vector<Region> Build(const std::vector<Region>& banks,
                          const std::vector<Region>& reserved,
                          size_t cap = MEMMAP_MAX_REGIONS) {
  std::vector<Region> out(std::max(cap, size_t{1}));
  size_t n = memmap_build(
      reinterpret_cast<const mem_region_t*>(banks.data()), banks.size(),
      reinterpret_cast<const mem_region_t*>(reserved.data()), reserved.size(),
      reinterpret_cast<mem_region_t*>(out.data()), out.size());
  if (n == MEMMAP_BUILD_OVERFLOW) return {};
  out.resize(n);
  return out;
}

std::vector<Region> Build(std::initializer_list<Region> banks,
                          std::initializer_list<Region> reserved,
                          size_t cap = MEMMAP_MAX_REGIONS) {
  return Build(std::vector<Region>(banks), std::vector<Region>(reserved),
               cap);
}

std::vector<Region> Build(const std::vector<Region>& banks,
                          std::initializer_list<Region> reserved,
                          size_t cap = MEMMAP_MAX_REGIONS) {
  return Build(banks, std::vector<Region>(reserved), cap);
}

/* QEMU virt shape: one 1 GiB bank, kernel image in the middle. */
std::vector<Region> QemuLike() {
  return {{0x40000000, 1024 * kMiB}};
}

}  // namespace

/* --- no reservations --- */

TEST(MemmapBuildTest, SingleBankNoReservations) {
  auto r = Build(QemuLike(), {});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, 1024 * kMiB);
}

TEST(MemmapBuildTest, EmptyInputYieldsEmpty) {
  auto r = Build({}, {});
  EXPECT_TRUE(r.empty());
}

/* --- alignment clamping --- */

TEST(MemmapBuildTest, BankClampedToWholePages) {
  /* base 0x801 into page 1, end 0x900 short of page 3's end: usable must
   * shrink to exactly one whole page (page 2). */
  auto r = Build({{kPage + 0x801, 3 * kPage - 0x801 - 0x900}}, {});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 2 * kPage);
  EXPECT_EQ(r[0].len, kPage);
}

TEST(MemmapBuildTest, SubPageBankDisappears) {
  auto r = Build({{0x1000, 0x800}}, {});
  EXPECT_TRUE(r.empty());
}

TEST(MemmapBuildTest, ReservationGrowsToWholePages) {
  /* reservation covering only part of a page removes the whole page */
  const uint64_t bank_end = 0x40000000 + 1024 * kMiB;
  const uint64_t rstart = 0x40000000 + kPage + 0x100; /* inside page 1 */
  auto r = Build(QemuLike(), {{rstart, 0x800}});
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, kPage);
  /* reservation grew to [0x40100000, 0x40101000) */
  EXPECT_EQ(r[1].base, 0x40000000 + 2 * kPage);
  EXPECT_EQ(r[1].len, bank_end - (0x40000000 + 2 * kPage));
}

/* --- multi-bank --- */

TEST(MemmapBuildTest, MultiBankSortedAndKept) {
  /* deliberately unsorted input */
  auto r = Build({{0x800000000, 2 * kMiB}, {0x40000000, kMiB}}, {});
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[1].base, 0x800000000u);
}

TEST(MemmapBuildTest, AbuttingBanksFuse) {
  auto r = Build({{0x40000000, kMiB}, {0x40100000, kMiB}}, {});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, 2 * kMiB);
}

TEST(MemmapBuildTest, OverlappingBanksFuse) {
  auto r = Build({{0x40000000, 2 * kMiB}, {0x40000000 + kMiB, 2 * kMiB}}, {});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, 3 * kMiB);
}

/* --- reservation shapes --- */

TEST(MemmapBuildTest, ReservationSplitsBank) {
  auto r = Build(QemuLike(), {{0x40000000 + 16 * kMiB, 4 * kMiB}});
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, 16 * kMiB);
  EXPECT_EQ(r[1].base, 0x40000000u + 20 * kMiB);
  EXPECT_EQ(r[1].len, 1024 * kMiB - 20 * kMiB);
}

TEST(MemmapBuildTest, ReservationAtBankStart) {
  auto r = Build(QemuLike(), {{0x40000000, 8 * kMiB}});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x40000000u + 8 * kMiB);
  EXPECT_EQ(r[0].len, 1024 * kMiB - 8 * kMiB);
}

TEST(MemmapBuildTest, ReservationAtBankEnd) {
  auto r = Build(QemuLike(), {{0x40000000 + 1016 * kMiB, 8 * kMiB}});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, 1016 * kMiB);
}

TEST(MemmapBuildTest, ReservationSwallowsBank) {
  auto r = Build({{0x40000000, kMiB},
                  {0x800000000, 2 * kMiB}},
                 {{0x3ff00000, 3 * kMiB}});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].base, 0x800000000u);
  EXPECT_EQ(r[0].len, 2 * kMiB);
}

TEST(MemmapBuildTest, ReservationSpanningMultipleBanks) {
  /* one reservation punching a hole through two banks with a gap */
  auto r = Build({{0x40000000, 2 * kMiB},
                  {0x40000000 + 3 * kMiB, 2 * kMiB}},
                 {{0x40000000 + kMiB, 3 * kMiB}});
  ASSERT_EQ(r.size(), 2u);
  EXPECT_EQ(r[0].base, 0x40000000u);
  EXPECT_EQ(r[0].len, kMiB);
  EXPECT_EQ(r[1].base, 0x40000000u + 4 * kMiB);
  EXPECT_EQ(r[1].len, kMiB);
}

TEST(MemmapBuildTest, KernelAndDtbCarveoutQemuShape) {
  /* the real boot shape: kernel image at +0x80000, DTB somewhere above it
   * (both loaders place it inside RAM, above the image) */
  const uint64_t bank_base = 0x40000000;
  const uint64_t bank_end = bank_base + 1024 * kMiB;
  const uint64_t kstart = 0x40080000, kend = 0x40240000;
  const uint64_t dbase = 0x41000000, dend = 0x41004000;
  auto r = Build(QemuLike(), {{kstart, kend - kstart},
                              {dbase, dend - dbase}});
  ASSERT_EQ(r.size(), 3u);
  /* below kernel */
  EXPECT_EQ(r[0].base, bank_base);
  EXPECT_EQ(r[0].len, kstart - bank_base);
  /* between kernel and dtb */
  EXPECT_EQ(r[1].base, kend);
  EXPECT_EQ(r[1].len, dbase - kend);
  /* above dtb to bank end */
  EXPECT_EQ(r[2].base, dend);
  EXPECT_EQ(r[2].len, bank_end - dend);
}

TEST(MemmapBuildTest, ZeroLengthReservationIgnored) {
  auto r = Build(QemuLike(), {{0x40100000, 0}});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].len, 1024 * kMiB);
}

TEST(MemmapBuildTest, ReservationOutsideBanksIgnored) {
  auto r = Build(QemuLike(), {{0x1000000, kMiB}});
  ASSERT_EQ(r.size(), 1u);
  EXPECT_EQ(r[0].len, 1024 * kMiB);
}

TEST(MemmapBuildTest, UnsortedReservationsSameResult) {
  std::vector<Region> a = Build(
      QemuLike(), {{0x40100000, kMiB}, {0x40000000, 0x1000}});
  std::vector<Region> b = Build(
      QemuLike(), {{0x40000000, 0x1000}, {0x40100000, kMiB}});
  EXPECT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size() && i < b.size(); i++) {
    EXPECT_EQ(a[i].base, b[i].base);
    EXPECT_EQ(a[i].len, b[i].len);
  }
}

/* --- capacity --- */

TEST(MemmapBuildTest, CapacityOverflowSignals) {
  /* 2 banks, reservations punching each into multiple fragments -> more
   * regions than the provided cap of 2 */
  std::vector<Region> banks = QemuLike();
  banks.push_back({0x800000000, 2 * kMiB});
  std::vector<Region> res = {
      {0x40000000 + kMiB, kPage},
      {0x40000000 + 100 * kMiB, kPage},
      {0x800000000 + kMiB, kPage},
      {0x800000000 + kMiB + kPage, kPage}};
  std::vector<Region> out(2);
  size_t n = memmap_build(
      reinterpret_cast<const mem_region_t*>(banks.data()), banks.size(),
      reinterpret_cast<const mem_region_t*>(res.data()), res.size(),
      reinterpret_cast<mem_region_t*>(out.data()), out.size());
  EXPECT_EQ(n, MEMMAP_BUILD_OVERFLOW);
  /* the same input fits with a full-size cap */
  EXPECT_FALSE(Build(banks, res).empty());
}

TEST(MemmapBuildTest, OutputIsSortedAndDisjoint) {
  std::vector<Region> res = {
      {0x40000000 + 32 * kMiB, kPage},
      {0x40000000 + 8 * kMiB, 2 * kPage},
      {0x40000000 + 200 * kMiB, kMiB},
      {0x40000000 + 199 * kMiB, 2 * kMiB} /* overlaps the one above */
  };
  auto r = Build(QemuLike(), res);
  ASSERT_GE(r.size(), 2u);
  for (size_t i = 0; i < r.size(); i++) {
    EXPECT_EQ(r[i].base % kPage, 0u);
    EXPECT_EQ(r[i].len % kPage, 0u);
    if (i > 0) EXPECT_GE(r[i].base, r[i - 1].base + r[i - 1].len);
  }
}
