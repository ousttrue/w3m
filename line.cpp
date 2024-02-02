#include "line.h"
#include "Str.h"
#include "alloc.h"
#include "lineprop.h"

Line::Line(int n, Line *prevl) : linenumber(n), prev(prevl) {
  if (prev) {
    prev->next = this;
  }
}

const char *NullLine = "";
Lineprop NullProp[] = {0};

Line::Line(int linenumber, Line *prevl, const char *line, Lineprop *prop,
           int byteLen, int realLinenumber)
    : linenumber(linenumber), prev(prevl), size(byteLen), len(byteLen) {
  if (prev) {
    prev->next = this;
  }

  if (byteLen > 0) {
    lineBuf = allocStr(line, byteLen);
    propBuf = (Lineprop *)NewAtom_N(Lineprop, byteLen);
    bcopy((void *)prop, (void *)propBuf, byteLen * sizeof(Lineprop));
  } else {
    lineBuf = (char *)NullLine;
    propBuf = NullProp;
  }

  if (realLinenumber < 0) {
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

  auto l = new Line(linenumber + 1, this);
  l->lineBuf = this->lineBuf + i;
  l->propBuf = this->propBuf + i;
  l->size = this->size - i;
  l->len = this->len - i;
  l->real_linenumber = this->real_linenumber;
  return l;
}
