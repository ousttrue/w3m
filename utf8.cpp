#include "utf8.h"
#include <locale>
#include <codecvt>
#include <string>
#include <string.h>
#include "myctype.h"
#include "widechar_width.h"
#include "Str.h"

enum class Utf8Bytes {
  Invalid,
  Lead1,
  Lead2,
  Lead3,
  Lead4,
  Successor,
};
Utf8Bytes detectByte(char8_t c) {
  if (c <= 0x7f) {
    // 0... ....
    return Utf8Bytes::Lead1;
  }
  if (c <= 0xBF) {
    // 10.. ....
    return Utf8Bytes::Successor;
  } else if (c <= 0xC1) {
    return Utf8Bytes::Invalid;
  } else if (c <= 0xDF) {
    // 110. ....
    return Utf8Bytes::Lead2;
  } else if (c <= 0xEF) {
    // 1110 ....
    return Utf8Bytes::Lead3;
  } else if (c <= 0xF4) {
    // 1111 0...
  } else {
    return Utf8Bytes::Invalid;
  }
}

std::string codepoint_to_utf8(char32_t codepoint) {
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
  return convert.to_bytes(&codepoint, &codepoint + 1);
}

int codepoint_to_width(char32_t wc) {
  // We render U+1F6E1 (🛡) with a width of 2,
  // but widechar_width says it has a width of 1 because Unicode classifies it
  // as "neutral".
  //
  // So we simply decide the width ourselves
  if (wc == 0x1F6E1) {
    return 2;
  }

  int width = widechar_wcwidth(wc);

  switch (width) {
  // Some sensible defaults
  case widechar_nonprint:
  case widechar_combining:
  case widechar_unassigned:
  case widechar_non_character:
    return 0;
  case widechar_ambiguous:
  case widechar_private_use:
    return 1;
  case widechar_widened_in_9:
    // Our renderer supports Unicode 9
    return 2;
  default:
    // Use the width widechar_width gave us.
    return width;
  }
}

int Utf8::cols() const {
  auto [cp, bytes] = codepoint();
  return codepoint_to_width(cp);
}

#define is_utf8_lead_byte(c) (((c) & 0xC0) != 0x80)

// byte位置の属性
Lineprop get_mctype(const char *_c) {
  auto c = *_c;
  if (c <= 0x7F) {
    if (!IS_CNTRL(c)) {
      return PC_ASCII;
    }
  }

  // TODO
  return PC_CTRL;
}

// 1文字のbytelength
int get_mclen(const char *c) {
  auto [codepoint, bytes] = Utf8::from((const char8_t *)c).codepoint();
  if (bytes == 0) {
    // fall back ISO-8859-1
    return 1;
  }
  return bytes;
}

// 1文字のculmun幅
int get_mcwidth(const char *c) {
  auto [codepoint, bytes] = Utf8::from((const char8_t *)c).codepoint();
  if (bytes == 0) {
    // fall back ISO-8859-1
    return 1;
  }
  return codepoint_to_width(codepoint);
}

int get_strwidth(const char *c) {
  int width = 0;
  while (*c) {
    auto [codepoint, bytes] = Utf8::from((const char8_t *)c).codepoint();
    width += codepoint_to_width(codepoint);
    if (bytes == 0) {
      break;
    }
    c += bytes;
  }
  return width;
}

int get_Str_strwidth(Str *c) {
  int width = 0;
  for (int i = 0; i < c->length;) {
    auto [codepoint, bytes] =
        Utf8::from((const char8_t *)c->ptr + i).codepoint();
    if (bytes == 0) {
      break;
    }
    width += codepoint_to_width(codepoint);
    i += bytes;
  }
  return width;
}
