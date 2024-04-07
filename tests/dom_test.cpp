#include <gtest/gtest.h>
#include "html_parser.h"
#include <memory>
#include <stack>

struct HtmlNode {
  HtmlToken token;
  std::list<std::shared_ptr<HtmlNode>> children;

  static std::shared_ptr<HtmlNode> create(const HtmlToken &token) {
    return std::make_shared<HtmlNode>(token);
  }
};

struct HtmlInsersionMode {
  using Result = std::tuple<bool, HtmlInsersionMode>;
  class Context {
    std::shared_ptr<HtmlNode> _document;
    std::stack<std::shared_ptr<HtmlNode>> _stack;

  public:
    Context() : _document(new HtmlNode) { pushOpenElement(_document); }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    std::shared_ptr<HtmlNode> document() const { return _document; }
    void pushOpenElement(const std::shared_ptr<HtmlNode> &node) {
      _stack.push(node);
    }
  };
  using HtmlTreeConstructionModeFunc = Result (*)(const HtmlToken &, Context &);
  HtmlTreeConstructionModeFunc insert;
};

/// 13.2.6.4.3 The "before head" insertion mode
HtmlInsersionMode::Result beforeHeadMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {

  switch (token.type) {}

  return {true, {}};
}

/// 13.2.6.4.2 The "before html" insertion mode
HtmlInsersionMode::Result beforeHtmlMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {
  //
  switch (token.type) {
  case Doctype:
    return {true, {}};
  case Comment: {
    c.document()->children.push_back(HtmlNode::create(token));
    return {true, {}};
  }
  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      return {true, {}};
    }
    break;
  }
  case Tag:
    if (token.isStartTag("html")) {
      c.pushOpenElement(HtmlNode::create(token));
      return {true, {beforeHeadMode}};
    }
    break;
  }

  // fallback
  c.pushOpenElement(HtmlNode::create(HtmlToken(Tag, "<html>")));
  return {false, {beforeHeadMode}};
}

/// 13.2.6.4.1 The "initial" insertion mode
HtmlInsersionMode::Result initialInsertionMode(const HtmlToken &token,
                                               HtmlInsersionMode::Context &c) {
  switch (token.type) {
  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      return {true, {}};
    }
    return {false, {}};
  }

  case Comment: {
    c.document()->children.push_back(HtmlNode::create(token));
    return {true, {}};
  }

  case Doctype: {
    return {true, {}};
  }

  default:
    return {false, {beforeHtmlMode}};
  }
}

struct TreeConstruction {
  HtmlInsersionMode insertionMode;
  HtmlInsersionMode::Context context;

  TreeConstruction() : insertionMode(initialInsertionMode) {}
  void push(const HtmlToken &token) {
    while (true) {
      auto [consumed, next_mode] = insertionMode.insert(token, context);
      if (next_mode.insert) {
        insertionMode = next_mode;
      }
      if (consumed) {
        break;
      }
    }
  }
};

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
