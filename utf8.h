#pragma once
#include <stdint.h>

uint32_t utf8sequence_to_codepoint(const uint8_t *utf8);
int utf8sequence_from_codepoint(uint32_t codepoint, uint8_t buf[4]);
int utf8sequence_len(const uint8_t *utf8);
int utf8codepoint_width(uint32_t codepoint);
int utf8sequence_width(const uint8_t *utf8);
int utf8str_width(const uint8_t *utf8);
