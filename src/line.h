#pragma once
#include <vector>
#include "html/lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line {
  std::vector<char> lineBuf;
  int len() const { return static_cast<int>(lineBuf.size()); }
  std::vector<Lineprop> propBuf;

private:
  // column width. cache
  mutable int _width = -1;

public:
  void PPUSH(char p, Lineprop c) {
    propBuf.push_back(p);
    lineBuf.push_back(c);
  }

  int push_mc(Lineprop prop, const char *str);

  int width(bool force = false) const {
    if (force || _width == -1) {
      this->_width = this->bytePosToColumn(this->lineBuf.size());
    }
    return this->_width;
  }

  void assign(const char *buf, const Lineprop *prop, int byteLen) {
    lineBuf.reserve(byteLen + 1);
    propBuf.reserve(byteLen + 1);
    if (byteLen > 0) {
      lineBuf.assign(buf, buf + byteLen);
      propBuf.assign(prop, prop + byteLen);
    }
    lineBuf.push_back(0);
    propBuf.push_back(0);
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
      this->propBuf[pos] |= PE_MARK;
    }
  }
};
