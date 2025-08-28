#pragma once
#include "line_prop.h"
#include "writer.h"

extern int highIntensityColors;

struct Cell {
    char str[8];
    l_prop prop;
};
// static_assert(sizeof(struct Cell) == 16, "Cell");
struct Frame {
    int lines;
    int cols;
    struct Cell* cells;
};

void refreshFrame(const struct Writer* writer, struct Frame* frame);
struct VirtualTerm;
void refresh(const struct VirtualTerm* vt, const struct Writer* writer);
void termBell(const struct Writer* writer);
void termClear(const struct Writer* writer);
