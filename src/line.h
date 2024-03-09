#pragma once
#include <vector>
#include <stdint.h>
#include "html/lineprop.h"

#define LINELEN 256 /* Initial line length */

class Line {
  std::vector<char> _lineBuf;
  std::vector<Lineprop> _propBuf;

public:
  Line() {}
  Line(const char *line, Lineprop *prop, int byteLen);
  ~Line() {}
  int len() const { return static_cast<int>(_lineBuf.size()); }
  const char *lineBuf() const { return _lineBuf.data(); }
  void lineBuf(int pos, char c) { _lineBuf[pos] = c; }
  const Lineprop *propBuf() const { return _propBuf.data(); }
  void propBufAdd(int pos, Lineprop p) { _propBuf[pos] |= p; }
  void propBufDel(int pos, Lineprop p) { _propBuf[pos] &= ~p; }
  void set_mark(int pos, int epos) {
    for (; pos < epos && pos < this->len(); pos++) {
      this->_propBuf[pos] |= PE_MARK;
    }
  }
  void PPUSH(char p, Lineprop c) {
    _propBuf.push_back(p);
    _lineBuf.push_back(c);
  }
  int push_mc(Lineprop prop, const char *str);
  void assign(const char *buf, const Lineprop *prop, int byteLen) {
    _lineBuf.reserve(byteLen + 1);
    _propBuf.reserve(byteLen + 1);
    if (byteLen > 0) {
      _lineBuf.assign(buf, buf + byteLen);
      _propBuf.assign(prop, prop + byteLen);
    }
    _lineBuf.push_back(0);
    _propBuf.push_back(0);
  }

private:
  // column width. cache
  mutable std::vector<int> _posEndColMap;
  void _update() const;
  void _pushColWidth(uint16_t col) const {
    if (_posEndColMap.size()) {
      col += _posEndColMap.back();
    }
    _posEndColMap.push_back(col);
  }

public:
  int width() const {
    _update();
    return _posEndColMap.back();
  }
  // byte pos to column
  int bytePosToColumn(int pos) const {
    _update();
    auto end = _posEndColMap[pos];
    for (int i = pos - 1; i >= 0; --i) {
      auto begin = _posEndColMap[i];
      if (begin != end) {
        return begin;
      }
    }
    return 0;
  }
  // column to byte pos
  int columnPos(int col) const {
    _update();
    auto begin = 0;
    int beginPos = 0;
    for (int i = 0; i < len(); ++i) {
      auto end = _posEndColMap[i];
      if (end >= col) {
        return beginPos;
      }
      if (end != begin) {
        begin = end;
        beginPos = i;
      }
    }
    return _posEndColMap.size() - 1;
  }
};
