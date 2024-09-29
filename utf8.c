#include "utf8.h"
#include <widechar_width_c.h>

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
