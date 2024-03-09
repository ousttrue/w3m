#pragma once
#include <vector>
#include "html/lineprop.h"

#define LINELEN 256 /* Initial line length */

class Line {
  std::vector<char> _lineBuf;
  std::vector<Lineprop> _propBuf;

  // column width. cache
  mutable int _width = -1;

public:
  const char *lineBuf() const { return _lineBuf.data(); }
  void lineBuf(int pos, char c) { _lineBuf[pos] = c; }
  const Lineprop *propBuf() const { return _propBuf.data(); }
  void propBufAdd(int pos, Lineprop p) { _propBuf[pos] |= p; }
  void propBufDel(int pos, Lineprop p) { _propBuf[pos] &= ~p; }
  int len() const { return static_cast<int>(_lineBuf.size()); }
  void PPUSH(char p, Lineprop c) {
    _propBuf.push_back(p);
    _lineBuf.push_back(c);
  }

  int push_mc(Lineprop prop, const char *str);

  int width(bool force = false) const {
    if (force || _width == -1) {
      this->_width = this->bytePosToColumn(this->_lineBuf.size());
    }
    return this->_width;
  }

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

  Line() {}
  Line(const char *line, Lineprop *prop, int byteLen);
  ~Line() {}

  // byte pos to column
  int bytePosToColumn(int pos) const;

  // column to byte pos
  int columnPos(int col) const;

  // column required byteLen
  int columnLen(int col) const;

  void set_mark(int pos, int epos) {
    for (; pos < epos && pos < this->len(); pos++) {
      this->_propBuf[pos] |= PE_MARK;
    }
  }
};
