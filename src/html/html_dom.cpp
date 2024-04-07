#include "html_dom.h"

/// 13.2.6.4.1 The "initial" insertion mode
HtmlInsersionMode::Result initialInsertionMode(const HtmlToken &token,
                                               HtmlInsersionMode::Context &c) {
  switch (token.type) {
  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      return {true, {}};
    }
    break;
  }

  case Comment: {
    c.document()->children.push_back(HtmlNode::create(token));
    return {true, {}};
  }

  case Doctype: {
    return {true, {}};
  }
  }

  return {false, {beforeHtmlMode}};
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

/// 13.2.6.4.3 The "before head" insertion mode
HtmlInsersionMode::Result beforeHeadMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {

  switch (token.type) {}

  return {true, {}};
}

//
// TreeConstruction
//
TreeConstruction::TreeConstruction() : insertionMode(initialInsertionMode) {}

void TreeConstruction::push(const HtmlToken &token) {
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
