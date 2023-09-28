
#include "string_view.hpp"

#include <gtest/gtest.h>

#include <cstring>

TEST(StringView, create) {
  util::StringView empty_str;

  EXPECT_TRUE(empty_str.size() == 0);
  EXPECT_TRUE(empty_str.c_str() == nullptr);

  util::StringView str1 = "aaa";

  EXPECT_EQ(str1.size(), 3);
  EXPECT_STREQ(str1.c_str(), "aaa");
}

TEST(StringView, append) {
  util::StringView str1 = "aaa";

  str1 += "bbb";

  EXPECT_EQ(str1.size(), 6);

  EXPECT_STREQ(str1.c_str(), "aaabbb");

  util::StringView str2 = "cccc";

  str1 += str2;

  EXPECT_EQ(str1.size(), 10);
  EXPECT_EQ(str1.size(), std::strlen(str1.c_str()));
  EXPECT_STREQ(str1.c_str(), "aaabbbcccc");
}

TEST(StringView, binary_op) {
  util::StringView str1 = "aaa";
  util::StringView str2 = "bbb";

  auto str3 = str1 + str2;

  EXPECT_EQ(str1.size(), 3);
  EXPECT_EQ(str2.size(), 3);
  EXPECT_EQ(str3.size(), 6);
  EXPECT_STREQ(str3.c_str(), "aaabbb");

  EXPECT_TRUE(str1 == "aaa");
  EXPECT_TRUE(str2 == "bbb");
  EXPECT_TRUE(str3 == "aaabbb");

  EXPECT_FALSE(str1 == "a");
  EXPECT_FALSE(str1 == "");

  util::StringView str4;

  EXPECT_TRUE(str4 == "");

  str1 = "abcde";

  EXPECT_EQ(str1.size(), 5);
  EXPECT_STREQ(str1.c_str(), "abcde");
}

TEST(StringView, charAt) {
  util::StringView str1 = "abcdefghijklmn";

  EXPECT_EQ(str1[0], 'a');
  EXPECT_EQ(str1[str1.size() - 1], 'n');
}

int main(int argc, const char** argv) {
  ::testing::InitGoogleTest(&argc, (char**)argv);

  return RUN_ALL_TESTS();
}
