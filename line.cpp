#include "line.h"

Line::Line(int n, Line *prevl) : linenumber(n), prev(prevl) {
  if (prev) {
    prev->next = this;
  }
}

Line::Line(int linenumber, Line *prevl, char *line, Lineprop *prop, int byteLen,
           int realLinenumber)
    : linenumber(linenumber), prev(prevl), lineBuf(line), propBuf(prop),
      size(byteLen), len(byteLen) {
  if (prev) {
    prev->next = this;
  }

  if (realLinenumber < 0) {
    /*     l->real_linenumber = l->linenumber;     */
    real_linenumber = 0;
  } else {
    real_linenumber = realLinenumber;
  }
}

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf, propBuf, len, pos, 0, false);
}

Line *Line::breakLine(int breakWidth) {
  int i = columnLen(this, breakWidth);
  if (i == 0) {
    i++;
  }
  if (size <= i) {
    return {};
  }
  this->len = i;
  this->bpos = prev->bpos + this->len;
  this->bwidth = prev->bwidth + this->width(true);

  return new Line(linenumber + 1, this, lineBuf + i, propBuf + i, size - i,
                  this->real_linenumber);
}
