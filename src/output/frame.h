#pragma once
#include "line_prop.h"

struct Cell {
    char str[8];
    l_prop prop;
};

struct Frame {
    int rows;
    int cols;
    struct Cell* cells;
};
