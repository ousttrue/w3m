#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct str_view {
    char* ptr;
    size_t len;
};
