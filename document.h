#pragma once
#include "line.h"
#include <stdbool.h>

struct Document {
    bool lineUpdated;
    struct Line* firstLine;
    struct Line* lastLine;
    // scroll top
    struct Line* topLine;
    // cursor line
    struct Line* currentLine;
};
