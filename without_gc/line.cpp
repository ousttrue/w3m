#include "line.h"
#include "utf8.h"
#include "lineprop.h"

bool FoldTextarea = false;

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

int Line::form_update_line(std::string_view str, int spos, int epos, int width,
                           int newline, int password) {
  if(str.size()){
    auto a=0;
  }
  int c_len = 1;
  int c_width = 1;
  int w = 0;
  int pos = 0;
  for (auto p = str.begin(); p != str.end() && w < width;) {
    auto c_type = get_mctype(&*p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        w++;
        pos++;
      }
    } else if (password) {
      w += c_width;
      pos += c_width;
      w += c_width;
      pos += c_len;
    }
    p += c_len;
  }
  pos += width - w;

  int len = this->len() + pos + spos - epos;
  // buf = (char *)New_N(char, len + 1);
  // buf[len] = '\0';
  // std::vector<Lineprop> prop(len);
  // memcpy(buf, line->lineBuf(), spos * sizeof(char));
  // memcpy(prop, line->propBuf(), spos * sizeof(Lineprop));
  auto buf = this->_lineBuf;
  auto prop = this->_propBuf;

  auto effect = CharEffect(this->propBuf()[spos]);
  w = 0;
  pos = spos;
  for (auto p = str.begin(); p != str.end() && w < width;) {
    auto c_type = get_mctype(&*p);
    if (c_type == PC_CTRL) {
      if (newline && *p == '\n')
        break;
      if (*p != '\r') {
        buf[pos] = password ? '*' : ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else if (password) {
      for (int i = 0; i < c_width; i++) {
        buf[pos] = '*';
        prop[pos] = effect | PC_ASCII;
        pos++;
        w++;
      }
    } else {
      buf[pos] = *p;
      prop[pos] = effect | c_type;
      pos++;
      w += c_width;
    }
    p += c_len;
  }
  for (; w < width; w++) {
    buf[pos] = ' ';
    prop[pos] = effect | PC_ASCII;
    pos++;
  }
  if (newline) {
    assert(false);
    // if (!FoldTextarea) {
    //   while (*p && *p != '\r' && *p != '\n')
    //     p++;
    // }
    // if (*p == '\r')
    //   p++;
    // if (*p == '\n')
    //   p++;
  }
  // *str = p;

  // memcpy(&buf[pos], &line->lineBuf()[epos],
  //        (line->len() - epos) * sizeof(char));
  // memcpy(&prop[pos], &line->propBuf()[epos],
  //        (line->len() - epos) * sizeof(Lineprop));

  this->assign(buf.data(), prop.data(), len);

  return pos;
}
