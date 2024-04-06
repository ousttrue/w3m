#include <gtest/gtest.h>
#include <entity.h>

TEST(EntityTest, conv_entity) {
  {
    EXPECT_EQ("<", conv_entity('<'));
    EXPECT_EQ("あ", conv_entity(U'あ'));
  }
}

TEST(EntityTest, getescapechar) {
  {
    const char *p = "&amp;";
    auto [e, pp] = getescapechar(p);
    EXPECT_EQ('&', *e);
    EXPECT_EQ(p + 5, pp.data());
  }
  {
    const char *p = "&#9312;";
    auto [e, pp] = getescapechar(p);
    EXPECT_EQ(U'①', *e);
    EXPECT_EQ(p + 7, pp.data());
  }
}

TEST(EntityTest, getescapecmd) {
  {
    const char *p = "&amp;";
    auto [e, pp] = getescapecmd(p);
    EXPECT_EQ("&", e);
    EXPECT_EQ(p + 5, pp.data());
  }
  {
    const char *p = "&#9312;";
    auto [e, pp] = getescapecmd(p);
    EXPECT_EQ("①", e);
    EXPECT_EQ(p + 7, pp.data());
  }
}
