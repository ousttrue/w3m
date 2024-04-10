#include <gtest/gtest.h>
#include "html_parser.h"
#include "entity.h"

TEST(HtmlTest, token) {
  EXPECT_TRUE(HtmlToken(Tag, R"(<html>)").isStartTag("html"));
  EXPECT_TRUE(HtmlToken(Tag, R"(<html lang="ja">)").isStartTag("html"));
  EXPECT_TRUE(HtmlToken(Tag, R"(</html>)").isEndTag("html"));
}

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
      EXPECT_EQ(HtmlToken(Character, "a"), token);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Character, " "), token);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Character, "b"), token);
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
    EXPECT_EQ(HtmlToken(Character, "&amp;"), token);
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, tag) {
  {
    auto src = "<p>";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Tag, "<p>"), token);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "</h1>";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Tag, "</h1>"), token);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "<a href='http://hoge.fuga' >";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Tag, src), token);
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, html) {
  {
    auto src = "<a href=nan>なん</a>";
    auto g = html_tokenize(src);
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Tag, "<a href=nan>"), token);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Character, "な"), token);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Character, "ん"), token);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(HtmlToken(Tag, "</a>"), token);
    }

    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, doctype) {
  {
    auto src = "<!DOCTYPE html>";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Doctype, src), token);
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, comment) {
  {
    auto src = "<!-- comment -->";
    auto g = html_tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Comment, src), token);
    EXPECT_FALSE(g.move_next());
  }
}
