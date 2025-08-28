#pragma once
#include "line_prop.h"

struct Cell {
    char str[8];
    l_prop prop;
};
// static_assert(sizeof(struct Cell) == 16, "Cell");
struct Frame {
    int rows;
    int cols;
    struct Cell* cells;
};

struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);
