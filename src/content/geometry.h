#pragma once
#include <stdbool.h>

// cursor position, term size... etc
struct Int2 {
    int x;
    int y;
};

struct Rect {
    struct Int2 offset;
    struct Int2 size;
};

struct VirtualTerm;
struct Buffer;

struct UI {
    struct Buffer* current_buffer;
    struct VirtualTerm* vt;
    bool use_graphic;
    struct Rect viewport;
    // viewport local position
    struct Int2 viewport_cursor;
    // term global position
    struct Int2 term_cursor;
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
