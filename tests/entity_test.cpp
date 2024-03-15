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
    auto q = p;
    EXPECT_EQ('&', getescapechar(&p));
    EXPECT_EQ(q + 5, p);
  }
}

TEST(EntityTest, getescapecmd) {
  {
    const char *p = "&amp;";
    auto q = p;
    EXPECT_EQ("&", getescapecmd(&p));
    EXPECT_EQ(q + 5, p);
  }
}
