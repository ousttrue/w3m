#include "line.h"
#include "utf8.h"
#include "lineprop.h"

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

void Line::_update() const {
  if (_posEndColMap.size() == _lineBuf.size()) {
    return;
  }

  _posEndColMap.clear();

  int i = 0;
  int j = 0;
  for (; i < len();) {
    if (_propBuf[i] & PC_CTRL) {
      int width = 0;
      if (_lineBuf[i] == '\t') {
        width = Tabstop - (j % Tabstop);
      } else if (_lineBuf[i] == '\n') {
        width = 1;
      } else if (_lineBuf[i] != '\r') {
        width = 2;
      }
      _pushCell(j, width);
      j += width;
      ++i;
    } else {
      auto utf8 = Utf8::from((const char8_t *)&_lineBuf[i]);
      auto [cp, bytes] = utf8.codepoint();
      if (bytes) {
        auto width = utf8.width();
        for (int k = 0; k < bytes; ++k) {
          _pushCell(j, width);
          ++i;
        }
        j += width;
      } else {
        // ?
        _pushCell(j, 1);
        j += 1;
        ++i;
      }
    }
  }
}
