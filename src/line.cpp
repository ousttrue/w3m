#include "line.h"
#include "utf8.h"
#include "html/lineprop.h"

Line::Line(int n, Line *prevl) : linenumber(n), prev(prevl) {
  if (prev) {
    prev->next = this;
  }
}

Line::Line(int linenumber, Line *prevl, const char *buf, Lineprop *prop,
           int byteLen)
    : linenumber(linenumber), prev(prevl), lineBuf(byteLen + 1),
      propBuf(byteLen + 1) {
  if (prev) {
    prev->next = this;
  }
  assign(buf, prop, byteLen);
}

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf.data(), propBuf.data(), lineBuf.size(), pos,
                           0, false);
}

int Line::columnPos(int column) const {
  int i = 1;
  for (; i < this->lineBuf.size(); i++) {
    if (this->bytePosToColumn(i) > column)
      break;
  }
  for (i--; i > 0 && this->propBuf[i] & PC_WCHAR2; i--)
    ;
  return i;
}

int Line::columnLen(int column) const {
  for (auto i = 0; i < this->lineBuf.size();) {
    auto j = ::bytePosToColumn(&this->lineBuf[i], &this->propBuf[i],
                               this->lineBuf.size(), i, 0, false);
    if (j > column)
      return i;
    auto utf8 = Utf8::from((const char8_t *)&this->lineBuf[i]);
    auto [cp, bytes] = utf8.codepoint();
    i += bytes;
  }
  return this->lineBuf.size();
}
