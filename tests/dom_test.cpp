#include <gtest/gtest.h>
#include "html_parser.h"
#include "html_dom.h"

TEST(HtmlTest, dom) {
  {
    auto src = "<p>a</p>";
    auto g = html_tokenize(src);
    TreeConstruction t;
    while (g.move_next()) {
      auto token = g.current_value();
      t.push(token);
    }
    EXPECT_EQ(&beforeHeadMode, t.insertionMode.insert);
  }
}
