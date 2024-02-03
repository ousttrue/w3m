#pragma once
#include <stddef.h>
#include <gc_cpp.h>
#include <vector>
#include "lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line : public gc_cleanup {
  long linenumber = 0;  /* on buffer */
  long real_linenumber; /* on file */
  Line *next = nullptr;
  Line *prev = nullptr;

  std::vector<char> lineBuf;
  std::vector<Lineprop> propBuf;

private:
  // column width. cache
  mutable int _width = -1;

public:
  int width(bool force = false) const {
    if (force || _width == -1) {
      this->_width = this->bytePosToColumn(this->len);
    }
    return this->_width;
  }

  int size() const { return lineBuf.size() - 1; }

  void assign(const char *buf, const Lineprop *prop, int byteLen) {
    lineBuf.reserve(byteLen + 1);
    propBuf.reserve(byteLen + 1);
    if (byteLen > 0) {
      lineBuf.assign(buf, buf + byteLen);
      propBuf.assign(prop, prop + byteLen);
    }
    lineBuf.push_back(0);
    propBuf.push_back(0);
    this->len = byteLen;
  }

  // line break
  int len = 0;
  // bpos += l->len;
  int bpos = 0;
  // bwidth += l->width;
  int bwidth = 0;

  Line(int n, Line *prevl = nullptr);
  // Line(int linenumber, Line *prevl, char *line, Lineprop *prop, int byteLen,
  //      int realLinenumber);
  Line(int linenumber, Line *prevl, const char *line, Lineprop *prop,
       int byteLen, int realLinenumber);
  ~Line() {}

  Line(const Line &) = delete;
  Line &operator=(const Line &) = delete;

  // byte pos to column
  int bytePosToColumn(int pos) const;

  // column to byte pos
  int columnPos(int col) const;

  // column required byteLen
  int columnLen(int col) const;

  Line *breakLine(int breakWidth);

  void set_mark(int pos, int epos) {
    for (; pos < epos && pos < this->size(); pos++) {
      this->propBuf[pos] |= PE_MARK;
    }
  }
};
