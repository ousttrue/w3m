#include <gtest/gtest.h>
#include "html_parser.h"
#include "html_dom.h"
#include "html_node.h"

TEST(HtmlDomTest, partial) {
  {
    auto src = "<p>a</p><div>あ</div>";
    HtmlParser tokenizer;
    auto g = tokenizer.tokenize(src);
    TreeConstruction t([&tokenizer](auto state) { tokenizer.state = {state}; });
    while (g.move_next()) {
      auto token = g.current_value();
      t.push(token);
    }
    t.push(HtmlToken(Eof));
    // EXPECT_EQ(&beforeHeadMode, t.insertionMode.insert);

    t.context.document()->print(std::cout);
    std::cout << "(" << (int)t.context.currentMode() << ")"
              << t.context.currentMode() << std::endl;
  }
}

TEST(HtmlDomTest, full) {
  {
    auto src = R"(<!doctype html>
<html lang="ja">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width" />
    <title>テストページ</title>
  </head>
  <body>
    <img src="images/firefox-icon.png" alt="テスト画像" />
  </body>
</html>
)";

    HtmlParser tokenizer;
    auto g = tokenizer.tokenize(src);
    TreeConstruction t([&tokenizer](auto state) { tokenizer.state = {state}; });
    while (g.move_next()) {
      auto token = g.current_value();
      t.push(token);
    }
    t.push(HtmlToken(Eof));
    // EXPECT_EQ(&beforeHeadMode, t.insertionMode.insert);

    t.context.document()->print(std::cout);
    std::cout << "(" << (int)t.context.currentMode() << ")"
              << t.context.currentMode() << std::endl;
  }
}
