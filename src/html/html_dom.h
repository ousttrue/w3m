#pragma once
#include "html_token.h"
#include <memory>
#include <stack>
#include <list>

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

/// 13.2.6.4.1 The "initial" insertion mode
HtmlInsersionMode::Result initialInsertionMode(const HtmlToken &token,
                                               HtmlInsersionMode::Context &c);
/// 13.2.6.4.2 The "before html" insertion mode
HtmlInsersionMode::Result beforeHtmlMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);
/// 13.2.6.4.3 The "before head" insertion mode
HtmlInsersionMode::Result beforeHeadMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);

struct TreeConstruction {
  HtmlInsersionMode insertionMode;
  HtmlInsersionMode::Context context;

  TreeConstruction();
  void push(const HtmlToken &token);
};
