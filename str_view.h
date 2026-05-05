#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct mutable_str_view {
    char* ptr;
    size_t len;
};

struct str_view {
    const char* ptr;
    size_t len;
};
