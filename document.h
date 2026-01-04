#pragma once
#include "line.h"
#include "image.h"
#include <stdbool.h>

struct AnchorList;
struct Document {
    short width;
    const char* title;
    enum wc_ces charset;
    wc_uint8 auto_detect;
    struct BufferPos* undo;

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
    /// summary of all lines linelen ?
    size_t trbyte;
    // cursor position
    int currentColumn;

    //
    // anchors
    //
    struct Url* baseURL;
    char* baseTarget;
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
    struct FormItemList* form_submit;
    struct Anchor* submit;

    //
    // frame
    //
    struct frameset* frameset;
    struct frameset_queue* frameQ;

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

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->COLS = (srcbuf)->COLS;   \
        (dstbuf)->LINES = (srcbuf)->LINES; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->topLine = (srcbuf)->topLine;             \
        (dstbuf)->currentLine = (srcbuf)->currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }
// #define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
// #define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)

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
void doc_nscroll(struct Document* doc, int n);
void doc_shiftvisualpos(struct Document* doc, int shift);
void doc_movL(struct Document* doc, int n);
void doc_movR(struct Document* doc, int n);
void doc_movD(struct Document* doc, int n);
void doc_movU(struct Document* doc, int n);
bool doc_prev_nonnull_line(struct Document* doc, struct Line* line);
bool doc_next_nonnull_line(struct Document* doc, struct Line* line);
void doc_goLine(struct Document* doc, const char* l);
int doc_cur_real_linenumber(struct Document* doc);
void doc_nextA(struct Document* doc, bool visited, struct Url *base_url);
void doc_prevA(struct Document* doc, bool visited, struct Url *base_url);
struct Anchor* doc_retrieveCurrentAnchor(struct Document* doc);
struct Anchor* doc_retrieveCurrentImg(struct Document* doc);
struct Anchor* doc_retrieveCurrentForm(struct Document* doc);
struct Anchor* doc_retrieveCurrentMap(struct Document* doc);
struct MapArea* doc_retrieveCurrentMapArea(struct Document* doc);
