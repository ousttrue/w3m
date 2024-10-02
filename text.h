#pragma once
#include <stdint.h>

bool is_boundary(const unsigned char *ch1, const unsigned char *ch2);

const char *convert_size(int64_t size, int usefloat);
const char *convert_size2(int64_t size1, int64_t size2, int usefloat);
