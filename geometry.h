#pragma once
#include <stddef.h>

struct Vec2 {
    size_t x;
    size_t y;
};

struct DocumentPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct DocumentPos* next;
    struct DocumentPos* prev;
};
