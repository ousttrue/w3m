#pragma once
#include <gc_cpp.h>
#include <vector>
#include "html/lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line : public gc_cleanup {
  friend struct LineLayout;

private:
  long linenumber = 0; /* on buffer */

public:
  Line *next = nullptr;
  Line *prev = nullptr;

  std::vector<char> lineBuf;
  int len() const { return static_cast<int>(lineBuf.size()); }
  std::vector<Lineprop> propBuf;

  Line *currentLineSkip(int offset) {
    Line *l = this;
    if (offset == 0) {
      return l;
    }
    if (offset > 0)
      for (int i = 0; i < offset && l->next != NULL; i++, l = l->next)
        ;
    else
      for (int i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
        ;
    return l;
  }

private:
  // column width. cache
  mutable int _width = -1;

public:
  int width(bool force = false) const {
    if (force || _width == -1) {
      this->_width = this->bytePosToColumn(this->lineBuf.size());
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
  }

  Line(int n, Line *prevl = nullptr);
  // Line(int linenumber, Line *prevl, char *line, Lineprop *prop, int byteLen,
  //      int realLinenumber);
  Line(int linenumber, Line *prevl, const char *line, Lineprop *prop,
       int byteLen);
  ~Line() {}

  Line(const Line &) = delete;
  Line &operator=(const Line &) = delete;

  // byte pos to column
  int bytePosToColumn(int pos) const;

  // column to byte pos
  int columnPos(int col) const;

  // column required byteLen
  int columnLen(int col) const;

  void set_mark(int pos, int epos) {
    for (; pos < epos && pos < this->size(); pos++) {
      this->propBuf[pos] |= PE_MARK;
    }
  }
};
