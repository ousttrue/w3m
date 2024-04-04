#pragma once
#include "html_command.h"
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <optional>

#define GENERAL_LIST_MAX (INT_MAX / 32)

struct GeneralList;
struct TextLine {
  std::string line;
  int pos = 0;
  TextLine(std::string_view line = {}, int pos = 0)
      : line(line.begin(), line.end()), pos(pos) {}
  void align(int width, AlignMode mode);
};

struct GeneralList {
  std::list<std::shared_ptr<TextLine>> _list;

  static std::shared_ptr<GeneralList> newGeneralList() {
    return std::make_shared<GeneralList>();
  }

  std::shared_ptr<TextLine> popValue() {
    if (_list.empty()) {
      return {};
    }
    auto f = _list.front();
    _list.pop_front();
    return f;
  }

  std::shared_ptr<TextLine> rpopValue() {
    if (_list.empty()) {
      return {};
    }
    auto f = _list.back();
    _list.pop_back();
    return f;
  }

  void delValue(const std::shared_ptr<TextLine> &item) {
    for (auto it = _list.begin(); it != _list.end(); ++it) {
      if (*it == item) {
        _list.erase(it);
        return;
      }
    }
  }

  void appendGeneralList(const std::shared_ptr<GeneralList> &tl2) {
    if (/*tl &&*/ tl2) {
      for (auto &item : tl2->_list) {
        _list.push_back(item);
      }
      tl2->_list.clear();
    }
  }
};

class LineFeed {
  std::shared_ptr<GeneralList> _tl_lp2;

public:
  LineFeed(const std::shared_ptr<GeneralList> &tl) : _tl_lp2(tl) {}

  std::optional<std::string> textlist_feed() {
    if (_tl_lp2 && _tl_lp2->_list.size()) {
      auto p = _tl_lp2->_list.front();
      _tl_lp2->_list.pop_front();
      return p->line;
    }
    return {};
  }
};
