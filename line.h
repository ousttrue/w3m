#pragma once
#include <stddef.h>
#include <gc_cpp.h>
#include "lineprop.h"

#define LINELEN 256 /* Initial line length */

struct Line : public gc_cleanup {
  long linenumber = 0;  /* on buffer */
  long real_linenumber; /* on file */
  Line *next = nullptr;
  Line *prev = nullptr;

  char *lineBuf = {};
  Lineprop *propBuf = 0;
  int size = 0;

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

  // line break
  int len = 0;
  // bpos += l->len;
  int bpos = 0;
  // bwidth += l->width;
  int bwidth = 0;

  Line(int n, Line *prevl = nullptr);
  Line(int linenumber, Line *prevl, char *line, Lineprop *prop, int byteLen,
       int realLinenumber);
  ~Line() {}

  Line(const Line &) = delete;
  Line &operator=(const Line &) = delete;

  // byte pos to column
  int bytePosToColumn(int pos) const;

  Line *breakLine(int breakWidth);
};

int columnPos(Line *line, int column);
int columnLen(Line *line, int column);
