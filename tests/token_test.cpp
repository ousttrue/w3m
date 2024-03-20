#include <gtest/gtest.h>
#include "stringtoken.h"

TEST(TokenTest, sloppy_parse_line) {
  auto src = " <a> ";

  stringtoken st(src);

  ASSERT_FALSE(st.sloppy_parse_line());
  EXPECT_EQ(1, st.pos());

  auto token = st.sloppy_parse_line();
  EXPECT_EQ("<a>", *token);
  EXPECT_EQ(4, st.pos());

  ASSERT_FALSE(st.sloppy_parse_line());
  EXPECT_EQ(5, st.pos());

  ASSERT_TRUE(st.is_end());
}
