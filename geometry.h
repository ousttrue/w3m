#pragma once
#include <stddef.h>
#include <stdbool.h>

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

struct Anchor {
    const char* url;
    const char* target;
    const char* referer;
    const char* title;
    unsigned char accesskey;
    struct BufferPoint start;
    struct BufferPoint end;
    int hseq;
    bool slave;
    short y;
    short rows;
    struct Image* image;
};
inline static int onAnchor(struct Anchor* a, struct BufferPoint bp)
{
    if (bpcmp(bp, a->start) < 0)
        return -1;
    if (bpcmp(a->end, bp) <= 0)
        return 1;
    return 0;
}

