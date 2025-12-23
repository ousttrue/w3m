#pragma once
#include "line.h"

struct Document {
    struct Line* firstLine;
    struct Line* lastLine;
    // scroll top
    struct Line* topLine;
    // cursor line
    struct Line* currentLine;
};
