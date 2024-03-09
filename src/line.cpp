#include "line.h"
#include "utf8.h"
#include "html/lineprop.h"

Line::Line(const char *buf, Lineprop *prop, int byteLen) {
  assign(buf, prop, byteLen);
}

int Line::push_mc(Lineprop prop, const char *str) {
  int len = get_mclen(str);
  assert(len);
  auto mode = get_mctype(str);
  this->PPUSH(mode | prop, *(str++));
  mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
  for (int i = 1; i < len; ++i) {
    this->PPUSH(mode | prop, *(str++));
  }
  return len;
}

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf(), propBuf(), len(), pos, 0, false);
}

int Line::columnPos(int column) const {
  int i = 1;
  for (; i < this->len(); i++) {
    if (this->bytePosToColumn(i) > column)
      break;
  }
  for (i--; i > 0 && this->_propBuf[i] & PC_WCHAR2; i--)
    ;
  return i;
}

int Line::columnLen(int column) const {
  for (auto i = 0; i < this->len();) {
    auto j = ::bytePosToColumn(&this->_lineBuf[i], &this->_propBuf[i],
                               this->len(), i, 0, false);
    if (j > column)
      return i;
    auto utf8 = Utf8::from((const char8_t *)&this->_lineBuf[i]);
    auto [cp, bytes] = utf8.codepoint();
    i += bytes;
  }
  return this->len();
}
