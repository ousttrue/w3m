#include "line.h"
#include "lineprop.h"

Line::Line(int n, Line *prevl) : linenumber(n), prev(prevl) {
  if (prev) {
    prev->next = this;
  }
}

Line::Line(int linenumber, Line *prevl, const char *buf, Lineprop *prop,
           int byteLen, int realLinenumber)
    : linenumber(linenumber), prev(prevl), lineBuf(byteLen + 1),
      propBuf(byteLen + 1), len(byteLen) {
  if (prev) {
    prev->next = this;
  }

  assign(buf, prop, byteLen);

  if (realLinenumber < 0) {
    real_linenumber = 0;
  } else {
    real_linenumber = realLinenumber;
  }
}

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf.data(), propBuf.data(), len, pos, 0, false);
}

Line *Line::breakLine(int breakWidth) {
  int i = columnLen(this, breakWidth);
  if (i == 0) {
    i++;
  }
  if (size() <= i) {
    return {};
  }
  this->len = i;
  this->bpos = prev->bpos + this->len;
  this->bwidth = prev->bwidth + this->width(true);

  auto l = new Line(linenumber + 1, this);
  l->assign(this->lineBuf.data() + i, this->propBuf.data() + i,
            this->size() - i);
  l->real_linenumber = this->real_linenumber;
  return l;
}
