#pragma once
#include "html_token.h"
#include <memory>
#include <stack>
#include <list>
#include <ostream>
#include <assert.h>

struct HtmlNode {
  HtmlToken token;
  std::list<std::shared_ptr<HtmlNode>> children;
  static std::shared_ptr<HtmlNode> create(const HtmlToken &token) {
    return std::make_shared<HtmlNode>(token);
  }
  void print(std::ostream &os, const std::string &indent = "") {
    os << indent << token.view << std::endl;
    for (auto &child : children) {
      child->print(os, indent + "  ");
    }
  }
};

struct HtmlInsersionMode {
  using Result = std::tuple<bool, HtmlInsersionMode>;
  class Context;
  using HtmlTreeConstructionModeFunc = Result (*)(const HtmlToken &, Context &);
  HtmlTreeConstructionModeFunc insert;

  class Context {
    /// 13.2.4.2 The stack of open elements
    std::stack<std::shared_ptr<HtmlNode>> _stack;
    /// 13.2.4.3 The list of active formatting elements

    /// 13.2.4.4 The element pointers

    /// 13.2.4.5 Other parsing state flags
    bool _scripting = false;
    bool _framesetOk = true;

    HtmlTreeConstructionModeFunc _originalInsertionMode;

    // output
    std::shared_ptr<HtmlNode> _document;

  public:
    Context() : _document(new HtmlNode(HtmlToken(Tag, "[document]"))) {
      // pushOpenElement(_document);
      _stack.push(_document);
    }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    std::shared_ptr<HtmlNode> document() const { return _document; }

  private:
    void pushOpenElement(const std::shared_ptr<HtmlNode> &node) {
      _stack.top()->children.push_back(node);
      _stack.push(node);
    }
    void pushOpenElement(const HtmlToken &token) {
      pushOpenElement(HtmlNode::create(token));
    }

  public:
    void setFramesetOk(bool enable) { _framesetOk = enable; }
    void popOpenElement(std::string_view validate = {}) { _stack.pop(); }
    void insertComment(const HtmlToken &token) {}
    void insertCharacter(const HtmlToken &token) {}
    void insertHtmlElement(const HtmlToken &token) {}
    void closeHtmlElement(const HtmlToken &token) {}
    void setOriginalInsertionMode(HtmlTreeConstructionModeFunc mode) {
      _originalInsertionMode = mode;
    }
    void parseError(const HtmlToken &token) {}
    void mergeAttribute(const HtmlToken &token) {}
    void reconstructActiveFormattingElements() {}
    void stop(){
      // TODO:
    }
  };
};

// https://html.spec.whatwg.org/multipage/parsing.html

/// 13.2.6.4.1 The "initial" insertion mode
HtmlInsersionMode::Result initialInsertionMode(const HtmlToken &token,
                                               HtmlInsersionMode::Context &c);
/// 13.2.6.4.2 The "before html" insertion mode
HtmlInsersionMode::Result beforeHtmlMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);
/// 13.2.6.4.3 The "before head" insertion mode
HtmlInsersionMode::Result beforeHeadMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);
/// 13.2.6.4.4 The "in head" insertion mode
HtmlInsersionMode::Result inHeadMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c);
/// 13.2.6.4.5 The "in head noscript" insertion mode
HtmlInsersionMode::Result inHeadNoscriptMode(const HtmlToken &token,
                                             HtmlInsersionMode::Context &c);
/// 13.2.6.4.6 The "after head" insertion mode
HtmlInsersionMode::Result afterHeadMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c);
/// 13.2.6.4.7 The "in body" insertion mode
HtmlInsersionMode::Result inBodyMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c);
/// 13.2.6.4.8 The "text" insertion mode
HtmlInsersionMode::Result textMode(const HtmlToken &token,
                                   HtmlInsersionMode::Context &c);
/// 13.2.6.4.9 The "in table" insertion mode
HtmlInsersionMode::Result tableMode(const HtmlToken &token,
                                    HtmlInsersionMode::Context &c);
/// 13.2.6.4.10 The "in table text" insertion mode
HtmlInsersionMode::Result tableTextMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c);
/// 13.2.6.4.11 The "in caption" insertion mode
HtmlInsersionMode::Result captionMode(const HtmlToken &token,
                                      HtmlInsersionMode::Context &c);
/// 13.2.6.4.12 The "in column group" insertion mode
HtmlInsersionMode::Result inColumnMode(const HtmlToken &token,
                                       HtmlInsersionMode::Context &c);
/// 13.2.6.4.13 The "in table body" insertion mode
HtmlInsersionMode::Result inTableBodyMode(const HtmlToken &token,
                                          HtmlInsersionMode::Context &c);
/// 13.2.6.4.14 The "in row" insertion mode
HtmlInsersionMode::Result inRowMode(const HtmlToken &token,
                                    HtmlInsersionMode::Context &c);
/// 13.2.6.4.15 The "in cell" insertion mode
HtmlInsersionMode::Result inCellMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c);
/// 13.2.6.4.16 The "in select" insertion mode
HtmlInsersionMode::Result inSelectMode(const HtmlToken &token,
                                       HtmlInsersionMode::Context &c);
/// 13.2.6.4.17 The "in select in table" insertion mode
HtmlInsersionMode::Result inSelectInTableMode(const HtmlToken &token,
                                              HtmlInsersionMode::Context &c);
/// 13.2.6.4.18 The "in template" insertion mode
HtmlInsersionMode::Result inTemplateMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);
/// 13.2.6.4.19 The "after body" insertion mode
HtmlInsersionMode::Result afterBodyMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c);
/// 13.2.6.4.20 The "in frameset" insertion mode
HtmlInsersionMode::Result inFramesetMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c);
/// 13.2.6.4.21 The "after frameset" insertion mode
HtmlInsersionMode::Result afterFramesetMode(const HtmlToken &token,
                                            HtmlInsersionMode::Context &c);
/// 13.2.6.4.22 The "after after body" insertion mode
HtmlInsersionMode::Result afterAfterBodyMode(const HtmlToken &token,
                                             HtmlInsersionMode::Context &c);
/// 13.2.6.4.23 The "after after frameset" insertion mode
HtmlInsersionMode::Result afterAfterFramesetMode(const HtmlToken &token,
                                                 HtmlInsersionMode::Context &c);

struct TreeConstruction {
  /// 13.2.4.1 The insertion mode
  HtmlInsersionMode insertionMode;
  HtmlInsersionMode::Context context;

  TreeConstruction();
  void push(const HtmlToken &token);
};

inline std::ostream &operator<<(std::ostream &os,
                                const HtmlInsersionMode &mode) {
  if (mode.insert == initialInsertionMode) {
    os << "initialInsertionMode";
  } else if (mode.insert == beforeHtmlMode) {
    os << "beforeHtmlMode";
  } else if (mode.insert == beforeHeadMode) {
    os << "beforeHeadMode";
  } else if (mode.insert == inHeadMode) {
    os << "inHeadMode";
  } else if (mode.insert == inHeadNoscriptMode) {
    os << "inHeadNoscriptMode";
  } else if (mode.insert == afterHeadMode) {
    os << "afterHeadMode";
  } else if (mode.insert == inBodyMode) {
    os << "inBodyMode";
  } else if (mode.insert == textMode) {
    os << "textMode";
  } else if (mode.insert == tableMode) {
    os << "tableMode";
  } else if (mode.insert == tableTextMode) {
    os << "tableTextMode";
  } else if (mode.insert == captionMode) {
    os << "captionMode";
  } else if (mode.insert == inColumnMode) {
    os << "inColumnMode";
  } else if (mode.insert == inTableBodyMode) {
    os << "inTableBodyMode";
  } else if (mode.insert == inRowMode) {
    os << "inRowMode";
  } else if (mode.insert == inCellMode) {
    os << "inCellMode";
  } else if (mode.insert == inSelectMode) {
    os << "inSelectMode";
  } else if (mode.insert == inSelectInTableMode) {
    os << "inSelectInTableMode";
  } else if (mode.insert == inTemplateMode) {
    os << "inTemplateMode";
  } else if (mode.insert == afterBodyMode) {
    os << "afterBodyMode";
  } else if (mode.insert == inFramesetMode) {
    os << "inFramesetMode";
  } else if (mode.insert == afterFramesetMode) {
    os << "afterFramesetMode";
  } else if (mode.insert == afterAfterBodyMode) {
    os << "afterAfterBodyMode";
  } else if (mode.insert == afterAfterFramesetMode) {
    os << "afterAfterFramesetMode";
  } else {
    os << "unknown";
  }

  return os;
}
