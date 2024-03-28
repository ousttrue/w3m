#pragma once
#include <list>
#include <memory>
#include <string>
#include <string_view>

#define GENERAL_LIST_MAX (INT_MAX / 32)
enum AlignMode {
  ALIGN_CENTER = 0,
  ALIGN_LEFT = 1,
  ALIGN_RIGHT = 2,
  ALIGN_MIDDLE = 4,
  ALIGN_TOP = 5,
  ALIGN_BOTTOM = 6,
};
bool toAlign(char *oval, AlignMode *align);

struct GeneralList;
struct Str;
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

  void appendTextLine(std::string_view line, int pos) {
    _list.push_back(std::make_shared<TextLine>(line, pos));
  }
};
