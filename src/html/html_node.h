#pragma once
#include "html_token.h"
#include "quote.h"
#include <memory>
#include <algorithm>
#include <iomanip>

/// 13.2.4.1 The insertion mode
enum class InsertionMode {
  None = 0,
  InitialInsertionMode = 1,
  BeforeHtmlMode = 2,
  BeforeHeadMode = 3,
  InHeadMode = 4,
  InHeadNoscriptMode = 5,
  AfterHeadMode = 6,
  InBodyMode = 7,
  TextMode = 8,
  TableMode = 9,
  TableTextMode = 10,
  CaptionMode = 11,
  InColumnMode = 12,
  InTableBodyMode = 13,
  InRowMode = 14,
  InCellMode = 15,
  InSelectMode = 16,
  InSelectInTableMode = 17,
  InTemplateMode = 18,
  AfterBodyMode = 19,
  InFramesetMode = 20,
  AfterFramesetMode = 21,
  AfterAfterBodyMode = 22,
  AfterAfterFramesetMode = 23,
};
// enum HtmlDomInsertionMode {
//   Initial,
//   BeforeHtml,
//   BeforeHead,
//   InHead,
//   InHeadNoscript,
//   AfterHead,
//   InBody,
//   Text,
//   InTable,
//   InTableText,
//   InCaption,
//   InColumnGroup,
//   InTableBody,
//   InRow,
//   InCell,
//   InSelect,
//   InSelectInTable,
//   InTemplate,
//   AfterBody,
//   InFrameset,
//   AfterFrameset,
//   AfterAfterBody,
//   AfterAfterFrameset,
// };
inline std::ostream &operator<<(std::ostream &os, InsertionMode mode) {
  switch (mode) {
  case InsertionMode::None:
    os << "None";
    break;
  case InsertionMode::InitialInsertionMode:
    os << "initialInsertionMode";
    break;
  case InsertionMode::BeforeHtmlMode:
    os << "BeforeHtmlMode";
    break;
  case InsertionMode::BeforeHeadMode:
    os << "BeforeHeadMode";
    break;
  case InsertionMode::InHeadMode:
    os << "InHeadMode";
    break;
  case InsertionMode::InHeadNoscriptMode:
    os << "InHeadNoscriptMode";
    break;
  case InsertionMode::AfterHeadMode:
    os << "AfterHeadMode";
    break;
  case InsertionMode::InBodyMode:
    os << "InBodyMode";
    break;
  case InsertionMode::TextMode:
    os << "TextMode";
    break;
  case InsertionMode::TableMode:
    os << "TableMode";
    break;
  case InsertionMode::TableTextMode:
    os << "TableTextMode";
    break;
  case InsertionMode::CaptionMode:
    os << "CaptionMode";
    break;
  case InsertionMode::InColumnMode:
    os << "InColumnMode";
    break;
  case InsertionMode::InTableBodyMode:
    os << "InTableBodyMode";
    break;
  case InsertionMode::InRowMode:
    os << "InRowMode";
    break;
  case InsertionMode::InCellMode:
    os << "InCellMode";
    break;
  case InsertionMode::InSelectMode:
    os << "InSelectMode";
    break;
  case InsertionMode::InSelectInTableMode:
    os << "InSelectInTableMode";
    break;
  case InsertionMode::InTemplateMode:
    os << "InTemplateMode";
    break;
  case InsertionMode::AfterBodyMode:
    os << "AfterBodyMode";
    break;
  case InsertionMode::InFramesetMode:
    os << "InFramesetMode";
    break;
  case InsertionMode::AfterFramesetMode:
    os << "AfterFramesetMode";
    break;
  case InsertionMode::AfterAfterBodyMode:
    os << "AfterAfterBodyMode";
    break;
  case InsertionMode::AfterAfterFramesetMode:
    os << "AfterAfterFramesetMode";
    break;
    break;
  }

  return os;
}

struct HtmlNode : std::enable_shared_from_this<HtmlNode> {
  InsertionMode mode;
  HtmlToken token;
  std::weak_ptr<HtmlNode> parent;
  std::list<std::shared_ptr<HtmlNode>> children;

  HtmlNode(InsertionMode mode, const HtmlToken &token)
      : mode(mode), token(token) {}
  HtmlNode() {}
  HtmlNode(const HtmlNode &) = delete;
  HtmlNode &operator=(const HtmlNode &) = delete;

  static std::shared_ptr<HtmlNode> create(InsertionMode mode,
                                          const HtmlToken &token) {
    return std::make_shared<HtmlNode>(mode, token);
  }

  void addChild(const std::shared_ptr<HtmlNode> &child) {
    child->parent = shared_from_this();
    this->children.push_back(child);
  }

  bool extendCharacter(const HtmlToken &token) {
    if (this->children.empty()) {
      return false;
    }
    auto &last = children.back();
    if (last->token.type != Character) {
      return false;
    }
    if (token.type != Character) {
      return false;
    }
    if (last->token.view.data() + last->token.view.size() !=
        token.view.data()) {
      return false;
    }

    // concat token
    last->token.view = {last->token.view.data(),
                        token.view.data() + token.view.size()};
    return true;
  }

  void print(std::ostream &os, const std::string &indent = "") {
    os
        //
        << std::setw(2) << std::setfill('0')
        << (int)this->mode
        //
        << indent;
    if (token.type == Character) {
      os << "|";
    }
    os << trim(token.view) << std::endl;
    // if (token.isStartTag("script")) {
    //   return;
    // }

    auto printTextRange = [&os, &indent, &children = this->children](auto begin,
                                                                     auto end) {
      if (begin != children.end()) {
        if (std::all_of(begin, end,
                        [](auto x) { return x->token.isspace(); })) {
          // skip
        } else {
          // TODO: trim
          for (auto it = begin; it != end; ++it) {
            (*it)->print(os, indent + "  ");
          }
        }
      }
    };

    std::list<std::shared_ptr<HtmlNode>>::iterator beginText =
        this->children.end();
    for (auto it = children.begin(); it != children.end(); ++it) {
      if ((*it)->token.type == Character) {
        if (beginText == children.end()) {
          beginText = it;
        }
      } else {
        if (beginText != children.end()) {
          printTextRange(beginText, it);
          beginText = children.end();
        }
        (*it)->print(os, indent + "  ");
      }
    }
    printTextRange(beginText, children.end());
  }

  bool hasParentTag(std::string_view tag) const {
    if (token.isStartTag(tag)) {
      return true;
    }
    if (auto p = parent.lock()) {
      return p->hasParentTag(tag);
    } else {
      return false;
    }
  }
};
