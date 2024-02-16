#include "line.h"
#include "utf8.h"
#include "html/lineprop.h"

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

int Line::columnPos(int column) const {
  int i = 1;
  for (; i < this->len; i++) {
    if (this->bytePosToColumn(i) > column)
      break;
  }
  for (i--; i > 0 && this->propBuf[i] & PC_WCHAR2; i--)
    ;
  return i;
}

int Line::columnLen(int column) const {
  for (auto i = 0; i < this->len;) {
    auto j = ::bytePosToColumn(&this->lineBuf[i], &this->propBuf[i], this->len,
                               i, 0, false);
    if (j > column)
      return i;
    auto utf8 = Utf8::from((const char8_t *)&this->lineBuf[i]);
    auto [cp, bytes] = utf8.codepoint();
    i += bytes;
  }
  return this->len;
}

Line *Line::breakLine(int breakWidth) {
  int i = columnLen(breakWidth);
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
