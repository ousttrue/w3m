#pragma once

// cursor position, term size... etc
struct Int2 {
    int x;
    int y;
};

struct Rect {
    struct Int2 offset;
    struct Int2 size;
};

struct BufferPoint {
    int line;
    int pos;
    int invalid;
};

struct BufferPos {
    int top_linenumber;
    int cur_linenumber;
    int currentColumn;
    int pos;
    struct BufferPos* next;
    struct BufferPos* prev;
};
