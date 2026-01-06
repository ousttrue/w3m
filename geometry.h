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

struct BufferPoint {
    int line;
    // column position ?
    int pos;
    int invalid;
};
inline static int bpcmp(struct BufferPoint a, struct BufferPoint b)
{
    return (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos));
}
