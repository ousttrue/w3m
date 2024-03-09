#include "line.h"
#include "utf8.h"
#include "html/lineprop.h"

Line::Line(const char *buf, Lineprop *prop, int byteLen) {
  assign(buf, prop, byteLen);
}

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf.data(), propBuf.data(), len(), pos, 0,
                           false);
}

int Line::columnPos(int column) const {
  int i = 1;
  for (; i < this->len(); i++) {
    if (this->bytePosToColumn(i) > column)
      break;
  }
  for (i--; i > 0 && this->propBuf[i] & PC_WCHAR2; i--)
    ;
  return i;
}

int Line::columnLen(int column) const {
  for (auto i = 0; i < this->len();) {
    auto j = ::bytePosToColumn(&this->lineBuf[i], &this->propBuf[i],
                               this->len(), i, 0, false);
    if (j > column)
      return i;
    auto utf8 = Utf8::from((const char8_t *)&this->lineBuf[i]);
    auto [cp, bytes] = utf8.codepoint();
    i += bytes;
  }
  return this->len();
}
