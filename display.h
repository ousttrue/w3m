#pragma once
#include "Line.h"

enum DisplayMode {
    B_NORMAL = 0,
    B_FORCE_REDRAW = 1,
    B_REDRAW = 2,
    B_SCROLL = 3,
    B_REDRAW_IMAGE = 4,
};

struct Buffer;
void displayBuffer(struct Buffer* buf, enum DisplayMode mode);
void calcTabPos(void);
