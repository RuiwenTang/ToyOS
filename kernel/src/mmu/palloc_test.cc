#include <gtest/gtest.h>
// after gtest
#include "mmu/palloc.h"

uint32_t free_base = 0x13000;
uint32_t free_size = 0x200000;

TEST(palloc, alloc_and_free) {
  palloc_init(free_base, free_size);

  EXPECT_TRUE(free_list() != nullptr);
  EXPECT_EQ(free_list()->base, free_base);
  EXPECT_EQ(free_list()->length, free_size);

  auto region1 = palloc_allocate(0x2000);

  EXPECT_EQ(region1, free_base);
  EXPECT_EQ(free_list()->base, free_base + 0x2000);
  EXPECT_EQ(free_list()->length, free_size - 0x2000);

  auto region2 = palloc_allocate(0x4000);

  EXPECT_EQ(region2, free_base + 0x2000);

  EXPECT_EQ(free_list()->base, free_base + 0x2000 + 0x4000);
  EXPECT_EQ(free_list()->length, free_size - 0x2000 - 0x4000);

  palloc_free(region1, 0x2000);

  EXPECT_EQ(free_list()->base, region1);
  EXPECT_EQ(free_list()->length, 0x2000);

  palloc_free(region2, 0x4000);

  EXPECT_EQ(free_list()->base, free_base);
  EXPECT_EQ(free_list()->length, free_size);
};

int main(int argc, const char** argv) {
  ::testing::InitGoogleTest(&argc, (char**)argv);

  return RUN_ALL_TESTS();
}
