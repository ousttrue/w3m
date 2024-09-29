#pragma once
#include <stdint.h>

int utf8sequence_from_codepoint(uint32_t codepoint, uint8_t buf[4]);
