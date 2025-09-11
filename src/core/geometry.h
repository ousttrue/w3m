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
