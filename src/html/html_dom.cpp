#include "html_dom.h"

// 1
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

// 2
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
      c.pushOpenElement(token);
      return {true, {beforeHeadMode}};
    }
    break;
  }

  // fallback
  c.pushOpenElement(HtmlNode::create(HtmlToken(Tag, "<html>")));
  return {false, {beforeHeadMode}};
}

// 3
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
    c.document()->children.push_back(HtmlNode::create(token));
    return {true, {}};
  }
  case Doctype: {
    return {true, {}};
  }
  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    if (token.isStartTag("head")) {
      c.pushOpenElement(token);
      return {true, {inHeadMode}};
    }
    break;
  }
  }

  c.pushOpenElement(HtmlNode::create(HtmlToken(Tag, "<head>")));
  return {false, {inHeadMode}};
}

// 4
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

  case Doctype:
    return {true, {}};

  case Tag: {
    if (token.isStartTag("html")) {
      return inBodyMode(token, c);
    }
    break;
  }

  } // switch

  c.popOpenElement();
  return {false, {afterHeadMode}};
}

/// 13.2.6.4.5 The "in head noscript" insertion mode
HtmlInsersionMode::Result inHeadNoscriptMode(const HtmlToken &token,
                                             HtmlInsersionMode::Context &c)
{
  return {true, {}};
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

  } // switch

  return {true, {}};
}

// 7
HtmlInsersionMode::Result inBodyMode(const HtmlToken &token,
                                     HtmlInsersionMode::Context &c) {
  switch (token.type) {
  case Character: {
    auto ch = token.view.front();
    if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
      if (auto e = c.activeFormattingElement()) {
        e->reconstruct();
      }
      c.insertCharacter(token);
      return {true, {}};
    }
    break;
  }

  } // switch

  return {true, {}};
}

// 8
HtmlInsersionMode::Result textMode(const HtmlToken &token,
                                   HtmlInsersionMode::Context &c) {
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
