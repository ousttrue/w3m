#pragma once
#include "line.h"
#include <stdbool.h>

struct AnchorList;
struct Document {
    //
    // lines
    //
    bool lineUpdated;
    struct Line* firstLine;
    struct Line* lastLine;
    // scroll top
    struct Line* topLine;
    // cursor line
    struct Line* currentLine;
    int allLine;
    // cursor position
    int currentColumn;

    //
    // anchors
    //
    struct AnchorList* href;

    //
    // screen
    //
    // viewport
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    short cursorX;
};

void addnewline(struct Document* doc, const char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines);
struct Url;
struct Line* redrawLine(struct Document* doc, struct Line* l, int i, struct Url* base_url);
