#include <gtest/gtest.h>
#include "quote.h"

TEST(Text, trim) {
  EXPECT_EQ("a", trim(" a"));
  EXPECT_EQ("a", trim("a "));
  EXPECT_EQ("a", trim(" a "));
  EXPECT_EQ("あ", trim(" あ"));
  EXPECT_EQ("あ", trim("あ "));
  EXPECT_EQ("あ", trim(" あ "));
  EXPECT_EQ("a b", trim(" a b "));
  EXPECT_EQ("あ い", trim(" あ い "));
}
