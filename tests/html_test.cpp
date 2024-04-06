#include <gtest/gtest.h>
#include "html_parser.h"
#include "entity.h"

TEST(HtmlTest, tokenizer) {
  {
    auto src = " ";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Character, src), token);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "a b";
    auto g = html_tokenize(src);
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("a", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(" ", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("b", token.view);
    }
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, named) {
  {
    auto src = "&amp;";
    auto [found, remain] = matchNamedCharacterReference(src + 1);
    EXPECT_EQ("amp;", found);
    EXPECT_EQ("", remain);
  }
}

TEST(HtmlTest, reference) {
  {
    auto src = "&amp;";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ("&amp;", token.view);
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, tag) {
  {
    auto src = "<p>";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ("<p>", token.view);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "</h1>";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ("</h1>", token.view);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "<a href='http://hoge.fuga' >";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(src, token.view);
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, html) {
  {
    auto src = "<a href=nan>なんだってー</a>";
    auto g = html_tokenize(src);
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("<a href=nan>", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("なんだってー", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("</a>", token.view);
    }

    EXPECT_FALSE(g.move_next());
  }
}
