#pragma once
#include "html_token.h"
#include "quote.h"
#include <memory>
#include <list>
#include <algorithm>
#include <iomanip>

struct HtmlNode : std::enable_shared_from_this<HtmlNode> {
  HtmlToken token;
  std::weak_ptr<HtmlNode> parent;
  std::list<std::shared_ptr<HtmlNode>> children;

  HtmlNode(const HtmlToken &token) : token(token) {}
  HtmlNode() {}
  HtmlNode(const HtmlNode &) = delete;
  HtmlNode &operator=(const HtmlNode &) = delete;

  static std::shared_ptr<HtmlNode> create(const HtmlToken &token) {
    return std::make_shared<HtmlNode>(token);
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
        << this->token.insertionMode
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
