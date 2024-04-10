#include "html_dom.h"
#include <assert.h>

inline InsertionMode getInsertionMode(intptr_t func) {
  if (func == (intptr_t)&initialInsertionMode) {
    return (InsertionMode)1;
  } else if (func == (intptr_t)&beforeHtmlMode) {
    return (InsertionMode)2;
  } else if (func == (intptr_t)&beforeHeadMode) {
    return (InsertionMode)3;
  } else if (func == (intptr_t)&inHeadMode) {
    return (InsertionMode)4;
  } else if (func == (intptr_t)&inHeadNoscriptMode) {
    return (InsertionMode)5;
  } else if (func == (intptr_t)&afterHeadMode) {
    return (InsertionMode)6;
  } else if (func == (intptr_t)&inBodyMode) {
    return (InsertionMode)7;
  } else if (func == (intptr_t)&textMode) {
    return (InsertionMode)8;
  } else if (func == (intptr_t)&tableMode) {
    return (InsertionMode)9;
  } else if (func == (intptr_t)&tableTextMode) {
    return (InsertionMode)10;
  } else if (func == (intptr_t)&captionMode) {
    return (InsertionMode)11;
  } else if (func == (intptr_t)&inColumnMode) {
    return (InsertionMode)12;
  } else if (func == (intptr_t)&inTableBodyMode) {
    return (InsertionMode)13;
  } else if (func == (intptr_t)&inRowMode) {
    return (InsertionMode)14;
  } else if (func == (intptr_t)&inCellMode) {
    return (InsertionMode)15;
  } else if (func == (intptr_t)&inSelectMode) {
    return (InsertionMode)16;
  } else if (func == (intptr_t)&inSelectInTableMode) {
    return (InsertionMode)17;
  } else if (func == (intptr_t)&inTemplateMode) {
    return (InsertionMode)18;
  } else if (func == (intptr_t)&afterBodyMode) {
    return (InsertionMode)19;
  } else if (func == (intptr_t)&inFramesetMode) {
    return (InsertionMode)20;
  } else if (func == (intptr_t)&afterFramesetMode) {
    return (InsertionMode)21;
  } else if (func == (intptr_t)&afterAfterBodyMode) {
    return (InsertionMode)22;
  } else if (func == (intptr_t)&afterAfterFramesetMode) {
    return (InsertionMode)23;
  }

  return {};
}

//
// HtmlInsersionMode::Context
//
HtmlInsersionMode::Context::Context()
    : _insertionMode(initialInsertionMode),
      _document(new HtmlNode({}, HtmlToken(Tag, "[document]"))) {
  // pushOpenElement(_document);
  _stack.push_front(_document);
}

InsertionMode HtmlInsersionMode::Context::currentMode() const {
  return getInsertionMode(reinterpret_cast<intptr_t>(this->_insertionMode));
}

void HtmlInsersionMode::Context::pushOpenElement(
    const std::shared_ptr<HtmlNode> &node) {
  _stack.front()->addChild(node);
  _stack.push_front(node);
}

void HtmlInsersionMode::Context::insertCharacter(const HtmlToken &token) {
  auto current = _stack.front();
  if (current->extendCharacter(token)) {
    // extended
  } else {
    current->addChild(HtmlNode::create(currentMode(), token));
  }
}

void HtmlInsersionMode::Context::insertHtmlElement(const HtmlToken &token) {
  pushOpenElement(HtmlNode::create(currentMode(), token));
}

void HtmlInsersionMode::Context::closeHtmlElement(const HtmlToken &token) {
  auto found = std::find_if(
      _stack.begin(), _stack.end(),
      [tag = token.tag()](auto node) { return node->token.tag() == tag; });
  if (found != _stack.end()) {
    ++found;
    _stack.erase(_stack.begin(), found);
  } else {
    assert(false);
  }
}

void HtmlInsersionMode::Context::setParserState(
    HtmlParserState::StateFunc state) {
  //
}

//
// impl InsersionMode
//
// 1 expect DOCTYPE
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
    c.insertComment(token);
    return {true, {}};
  }

  case Doctype: {
    // TODO:
    return {true, {beforeHtmlMode}};
  }

  } // switch

  // Anything else
  return {false, {beforeHtmlMode}};
}

// 2 expect <html>
HtmlInsersionMode::Result beforeHtmlMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {
  //
  switch (token.type) {
  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Comment: {
    c.insertComment(token);
    return {true, {}};
  }

  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      return {true, {}};
    }
    break;
  }

  case Tag: {
    if (token.isStartTag("html")) {
      c.insertHtmlElement(token);
      return {true, {beforeHeadMode}};
    }
    if (token.isAnyEndTag("head", "body", "html", "br")) {
      break;
    }
    if (token.isEndTag()) {
      c.parseError(token);
      return {true, {}};
    }
    break;
  }

  } // switch

  // Anything else
  c.insertHtmlElement(HtmlToken(Tag, "<html> inserted"));
  return {false, {beforeHeadMode}};
}

// 3 expect <head>
HtmlInsersionMode::Result beforeHeadMode(const HtmlToken &token,
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
    c.insertComment(token);
    return {true, {}};
  }

  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    if (token.isStartTag("head")) {
      c.insertHtmlElement(token);
      return {true, {inHeadMode}};
    }
    if (token.isAnyEndTag("head", "body", "html", "br")) {
      break;
    }
    if (token.isEndTag()) {
      c.parseError(token);
      return {true, {}};
    }
    break;
  }

  } // switch

  // Anything else
  c.insertHtmlElement(HtmlToken(Tag, "<head> inserted"));
  return {false, {inHeadMode}};
}

// 4 expect </head>
HtmlInsersionMode::Result inHeadMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c) {
  switch (token.type) {

  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      c.insertCharacter(token);
      return {true, {}};
    }
    break;
  }

  case Comment: {
    c.insertComment(token);
    return {true, {}};
  }

  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    if (token.isAnyStartTag("base", "basefont", "bgsound", "link")) {
      c.insertHtmlElement(token);
      c.closeHtmlElement(token);
      return {true, {}};
    }
    if (token.isStartTag("meta")) {
      c.insertHtmlElement(token);
      c.closeHtmlElement(token);
      // TODO:
      return {true, {}};
    }
    if (token.isStartTag("title")) {
      c.insertHtmlElement(token);
      c.setParserState(rcdataState);
      c.setOriginalInsertionMode(&inHeadMode);
      return {true, {textMode}};
    }
    if (token.isStartTag("noscript")) {
      // scripting flag is disabled
      c.insertHtmlElement(token);
      return {true, {inHeadNoscriptMode}};
    }
    if (token.isStartTag("script")) {
      c.insertHtmlElement(token);
      // TODO: Switch the tokenizer to the script data state.
      c.setOriginalInsertionMode(&inHeadMode);
      return {true, {textMode}};
    }
    if (token.isEndTag("head")) {
      c.closeHtmlElement(token);
      return {true, {afterHeadMode}};
    }
    if (token.isAnyEndTag("body", "html", "br")) {
      break;
    }
    if (token.isStartTag("template")) {
      // ignore
      return {true, {}};
    }
    if (token.isEndTag("template")) {
      // ignore
      return {true, {}};
    }
    if (token.isStartTag("head")) {
      c.parseError(token);
      return {true, {}};
    }
    if (token.isEndTag()) {
      c.parseError(token);
      return {true, {}};
    }
    break;
  }

  } // switch

  // Anything else
  c.popOpenElement("head");
  return {false, {afterHeadMode}};
}

/// 13.2.6.4.5 The "in head noscript" insertion mode
HtmlInsersionMode::Result inHeadNoscriptMode(const HtmlToken &token,
                                             HtmlInsersionMode::Context &c) {
  switch (token.type) {
  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    if (token.isEndTag("noscript")) {
      c.popOpenElement("noscript");
      return {true, {inHeadMode}};
    }
    if (token.isAnyStartTag("basefont", "bgsound", "link", "meta", "noframes",
                            "style")) {
      return inHeadMode(token, c);
    }
    if (token.isEndTag("br")) {
      break;
    }
    if (token.isAnyStartTag("head", "noscript")) {
      c.parseError(token);
      return {true, {}};
    }
    if (token.isEndTag()) {
      c.parseError(token);
      return {true, {}};
    }
    break;
  }

  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      return inHeadMode(token, c);
    }
    break;
  }

  case Comment: {
    return inHeadMode(token, c);
  }

  } // switch

  // Anything else
  c.parseError(token);
  c.popOpenElement("noscript");
  return {false, {inHeadMode}};
}

// 6
HtmlInsersionMode::Result afterHeadMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c) {
  switch (token.type) {
  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      c.insertCharacter(token);
      return {true, {}};
    }
    break;
  }

  case Comment: {
    c.insertComment(token);
    return {true, {}};
  }

  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    if (token.isStartTag("body")) {
      c.insertHtmlElement(token);
      c.setFramesetOk(false);
      return {true, {inBodyMode}};
    }
    if (token.isStartTag("frameset")) {
      c.insertHtmlElement(token);
      return {true, {inFramesetMode}};
    }
    if (token.isAnyStartTag("base", "basefont", "bgsound", "link", "meta",
                            "noframes", "script", "style", "template",
                            "title")) {
      c.parseError(token);
      // TODO: head element pointer
      return {true, {}};
    }
    if (token.isEndTag("template")) {
      return inHeadMode(token, c);
    }
    if (token.isAnyEndTag("body", "html", "br")) {
      break;
    }
    if (token.isStartTag("head")) {
      c.parseError(token);
      return {true, {}};
    }
    if (token.isEndTag()) {
      c.parseError(token);
      return {true, {}};
    }

    break;
  }

  } // switch

  // Anything else
  c.insertHtmlElement(HtmlToken(Tag, "<body> inserted"));
  return {false, {inBodyMode}};
}

// 7
HtmlInsersionMode::Result inBodyMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c) {
  switch (token.type) {

  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      c.reconstructActiveFormattingElements();
      c.insertCharacter(token);
      return {true, {}};
    } else {
      c.reconstructActiveFormattingElements();
      c.insertCharacter(token);
      c.setFramesetOk(false);
      return {true, {}};
    }
    break;
  }

  case Comment: {
    c.insertComment(token);
    return {true, {}};
  }

  case Doctype: {
    c.parseError(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isStartTag("html")) {
      c.parseError(token);
      c.mergeAttribute(token);
      return {true, {}};
    }
    if (token.isAnyStartTag("base", "basefont", "bgsound", "link", "meta",
                            "noframes", "script", "style", "template",
                            "title")) {
      return inHeadMode(token, c);
    }
    if (token.isEndTag("template")) {
      return inHeadMode(token, c);
    }
    if (token.isStartTag("body")) {
      c.parseError(token);
      // TODO:
      c.mergeAttribute(token);
      return {true, {}};
    }
    if (token.isStartTag("frameset")) {
      c.parseError(token);
      // TODO:
      return {true, {}};
    }
    if (token.isEndTag("body")) {
      // TODO:
      return {true, {afterBodyMode}};
    }
    if (token.isEndTag("html")) {
      // TODO:
      return {false, {afterBodyMode}};
    }
    if (token.isAnyStartTag("address", "article", "aside", "blockquote",
                            "center", "details", "dialog", "dir", "div", "dl",
                            "fieldset", "figcaption", "figure", "footer",
                            "header", "hgroup", "main", "menu", "nav", "ol",
                            "p", "search", "section", "summary", "ul")) {
      // TODO: close p in button scope
      c.insertHtmlElement(token);
      return {true, {}};
    }
    if (token.isAnyStartTag("h1", "h2", "h3", "h4", "h5", "h6")) {
      // TODO: close p in button scope
      // TODO: nested hx
      c.insertHtmlElement(token);
      return {true, {}};
    }
    if (token.isAnyStartTag("pre", "listing")) {
      // TODO: close p in button scope
      c.insertHtmlElement(token);
      // TODO: ignore next 0x0A
      c.setFramesetOk(false);
      return {true, {}};
    }
    // if(token.isStartTag("form")){
    // }

    // Any other start tag
    if (token.isStartTag()) {
      c.reconstructActiveFormattingElements();
      c.insertHtmlElement(token);
      return {true, {}};
    }
    if (token.isEndTag()) {
      c.closeHtmlElement(token);
      return {true, {}};
    }

    break;
  }

  case Eof: {
    c.stop();
    return {true, {}};
  }

  } // switch

  // not reach here
  assert(false);
  return {true, {}};
}

// 8
HtmlInsersionMode::Result textMode(const HtmlToken &token,
                                   HtmlInsersionMode::Context &c) {
  switch (token.type) {

  case Character: {
    c.insertCharacter(token);
    return {true, {}};
  }

  case Tag: {
    if (token.isEndTag()) {
      c.closeHtmlElement(token);
    }
    return {true, {c.originalInsertionMode()}};
    break;
  }

  } // switch

  // TODO:
  assert(false);
  return {true, {}};
}

/// 13.2.6.4.9 The "in table" insertion mode
HtmlInsersionMode::Result tableMode(const HtmlToken &token,
                                    HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.10 The "in table text" insertion mode
HtmlInsersionMode::Result tableTextMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.11 The "in caption" insertion mode
HtmlInsersionMode::Result captionMode(const HtmlToken &token,
                                      HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.12 The "in column group" insertion mode
HtmlInsersionMode::Result inColumnMode(const HtmlToken &token,
                                       HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.13 The "in table body" insertion mode
HtmlInsersionMode::Result inTableBodyMode(const HtmlToken &token,
                                          HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.14 The "in row" insertion mode
HtmlInsersionMode::Result inRowMode(const HtmlToken &token,
                                    HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.15 The "in cell" insertion mode
HtmlInsersionMode::Result inCellMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.16 The "in select" insertion mode
HtmlInsersionMode::Result inSelectMode(const HtmlToken &token,
                                       HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.17 The "in select in table" insertion mode
HtmlInsersionMode::Result inSelectInTableMode(const HtmlToken &token,
                                              HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.18 The "in template" insertion mode
HtmlInsersionMode::Result inTemplateMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.19 The "after body" insertion mode
HtmlInsersionMode::Result afterBodyMode(const HtmlToken &token,
                                        HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.20 The "in frameset" insertion mode
HtmlInsersionMode::Result inFramesetMode(const HtmlToken &token,
                                         HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.21 The "after frameset" insertion mode
HtmlInsersionMode::Result afterFramesetMode(const HtmlToken &token,
                                            HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.22 The "after after body" insertion mode
HtmlInsersionMode::Result afterAfterBodyMode(const HtmlToken &token,
                                             HtmlInsersionMode::Context &c) {
  return {true, {}};
}
/// 13.2.6.4.23 The "after after frameset" insertion mode
HtmlInsersionMode::Result
afterAfterFramesetMode(const HtmlToken &token, HtmlInsersionMode::Context &c) {
  return {true, {}};
}

//
// TreeConstruction
//
TreeConstruction::TreeConstruction(const SetHtmlTokenizerStateFunc &func) {}

void TreeConstruction::push(const HtmlToken &token) {
  while (true) {
    // token.insertionMode =
    // getInsertionModeIndex((intptr_t)insertionMode.insert);
    auto consumed = context.process(token);
    if (consumed) {
      break;
    }
  }
}
