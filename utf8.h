#pragma once
#include "line.h"
#include <string_view>
#include <string>
#include <string.h>
#include <tuple>

#ifdef __clang__
typedef unsigned char char8_t;
#endif

std::string codepoint_to_utf8(char32_t codepoint);

// Lineprop get_mctype(unsigned char c);
//
// // 1文字のbytelength
// int get_mclen(const char8_t *c);
//
// // 1文字のculmun幅
// int get_mcwidth(const char8_t *c);
//
// // 文字列のcolumn幅
// int get_strwidth(const char8_t *c);
//
// // 文字列のcolumn幅
// int get_Str_strwidth(struct Str *c);

// TODO: for utf-8 or ISO-8859-1
#define get_mctype(c) (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII)
#define get_mclen(c) 1
#define get_mcwidth(c) 1
#define get_strwidth(c) strlen(c)
#define get_Str_strwidth(c) ((c)->length)

struct Utf8 {
  char8_t b0 = 0;
  char8_t b1 = 0;
  char8_t b2 = 0;
  char8_t b3 = 0;

  const char8_t *begin() const { return &b0; }

  const char8_t *end() const {
    if (b0 == 0) {
      return &b0;
    } else if (b1 == 0) {
      return &b1;
    } else if (b2 == 0) {
      return &b2;
    } else if (b3 == 0) {
      return &b3;
    } else {
      return (&b3) + 1;
    }
  }

  std::string_view view() const {
    return std::string_view((const char *)begin(),
                            std::distance(begin(), end()));
  }

  std::tuple<char32_t, int> codepoint() const {
    if (b0 == 0) {
      return {-1, 0};
    }
    if (b0 >= 0 && b0 <= 127) {
      // ascii
      return {b0, 1};
    }
    if (b1 == 0) {
      return {-1, 0};
    }
    if (b0 >= 192 && b0 <= 223) {
      return {(b0 - 192) * 64 + (b1 - 128), 2};
    }
    if (b0 == 0xed && (b1 & 0xa0) == 0xa0) {
      return {-1, 0}; // code points, 0xd800 to 0xdfff
    }
    if (b2 == 0) {
      return {-1, 0};
    }
    if (b0 >= 224 && b0 <= 239) {
      return {(b0 - 224) * 4096 + (b1 - 128) * 64 + (b2 - 128), 3};
    }
    if (b3 == 0) {
      return {-1, 0};
    }
    if (b0 >= 240 && b0 <= 247) {
      return {(b0 - 240) * 262144 + (b1 - 128) * 4096 + (b2 - 128) * 64 +
                  (b3 - 128),
              4};
    }
    return {-1, 0};
  }

  int cols() const;

  bool operator==(const Utf8 &rhs) const { return view() == rhs.view(); }
  bool operator!=(const Utf8 &rhs) const { return !(*this == rhs); }
};

inline Utf8 to_utf8(char32_t cp) {
  if (cp < 0x80) {
    return {
        (char8_t)cp,
    };
  } else if (cp < 0x800) {
    return {(char8_t)(cp >> 6 | 0x1C0), (char8_t)((cp & 0x3F) | 0x80)};
  } else if (cp < 0x10000) {
    return {
        (char8_t)(cp >> 12 | 0xE0),
        (char8_t)((cp >> 6 & 0x3F) | 0x80),
        (char8_t)((cp & 0x3F) | 0x80),
    };
  } else if (cp < 0x110000) {
    return {
        (char8_t)(cp >> 18 | 0xF0),
        (char8_t)((cp >> 12 & 0x3F) | 0x80),
        (char8_t)((cp >> 6 & 0x3F) | 0x80),
        (char8_t)((cp & 0x3F) | 0x80),
    };
  } else {
    return {0xEF, 0xBF, 0xBD};
  }
}
