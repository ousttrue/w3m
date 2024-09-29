#include "utf8.h"
#include <widechar_width_c.h>

struct Utf8 SPACE = {' ', 0, 0, 0};

int utf8codepoint_width(uint32_t codepoint) {
  return widechar_wcwidth(codepoint);
}

int utf8sequence_width(const uint8_t *utf8) {
  return utf8codepoint_width(utf8sequence_to_codepoint(utf8));
}

int utf8str_width(const uint8_t *utf8) {
  int w = 0;
  for (auto p = utf8; *p;) {
    w += utf8sequence_width(p);
    auto len = utf8sequence_len(p);
    if (len == 0) {
      // infinite loop
      break;
    }
    p += len;
  }
  return w;
}

bool utf8is_equals(struct Utf8 l, struct Utf8 r) {
  auto llen = utf8sequence_len(&l.c0);
  auto rlen = utf8sequence_len(&r.c0);
  if (llen != rlen) {
    return false;
  }
  if (llen == 0) {
    return false;
  }
  for (int i = 0; i < llen; ++i) {
    if ((&l.c0)[i] != (&r.c0)[i]) {
      return false;
    }
  }
  return true;
}
