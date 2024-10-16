#pragma once
#include <stdint.h>

struct Utf8 {
  uint8_t c0;
  uint8_t c1;
  uint8_t c2;
  uint8_t c3;
  uint8_t z;
};
extern struct Utf8 SPACE;

uint32_t utf8sequence_to_codepoint(const uint8_t *utf8);
int utf8sequence_from_str(const uint8_t *utf8, struct Utf8 *pOut);
int utf8sequence_from_codepoint(uint32_t codepoint, struct Utf8 *pOut);
int utf8sequence_len(const uint8_t *utf8);
int utf8sequence_width(const uint8_t *utf8);
bool utf8sequence_equals(struct Utf8 l, struct Utf8 r);

int utf8codepoint_width(uint32_t codepoint);

int utf8str_width(const uint8_t *utf8);
int utf8str_codepoint_count(const uint8_t *utf8);
int utf8str_utf16str(const uint8_t *utf8, uint16_t *pOut, size_t len);
