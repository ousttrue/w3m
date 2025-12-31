#pragma once
#include "line.h"
#include "image.h"
#include <stdbool.h>

struct AnchorList;
struct Document {
    const char* title;
    enum wc_ces charset;

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
    struct AnchorList* img;
    struct AnchorList* name;
    struct AnchorList* formitem;
    struct LinkList* linklist;
    struct FormList* formlist;
    struct MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;

    enum ImageGetFlags image_flag;

    //
    // screen
    //
    // viewport
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
};

#define TOP_LINENUMBER(doc) ((doc)->topLine ? (doc)->topLine->linenumber : 1)
#define CUR_LINENUMBER(doc) ((doc)->currentLine ? (doc)->currentLine->linenumber : 1)

struct Url;

void doc_addnewline(struct Document* doc, const char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines);
struct Line* doc_redrawLine(struct Document* doc, struct Line* l, int i, struct Url* base_url);
struct Line* doc_lineSkip(struct Document* doc, struct Line* line, int offset);

/// update cursorY, cursorX and pos from currentLine and currentColumn
void doc_arrangeLine(struct Document* doc);
/// update cursorY, cursorX pos and visualX
void doc_arrangeCursor(struct Document* doc);

void doc_cursorUpDown(struct Document* doc, int n);
void doc_cursorUp0(struct Document* doc, int n);
void doc_cursorDown0(struct Document* doc, int n);
/// set currentLine to line that has the number
void doc_gotoLine(struct Document* doc, int linenumer);
/// update currentColumn
int doc_columnSkip(struct Document* doc, int offset);

void doc_cursorHome(struct Document* doc);
void doc_cursorLeft(struct Document* doc, int n);
void doc_cursorRight(struct Document* doc, int n);
void doc_cursorDown(struct Document* doc, int n);
void doc_cursorUp(struct Document* doc, int n);
void doc_cursorXY(struct Document* doc, int x, int y);
void doc_restorePosition(struct Document* doc, struct Document* orig);
void doc_gotoRealLine(struct Document* doc, int n);
