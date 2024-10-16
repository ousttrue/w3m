// rendererd document
#pragma once
#include "line.h"

extern bool nextpage_topline;
extern bool showLineNum;
#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
extern bool FoldLine;
int FOLD_BUFFER_WIDTH();

struct AnchorList;
struct LinkList;
struct FormList;
struct MapList;
struct HmarkerList;

struct BufferPos {
  long top_linenumber;
  long cur_linenumber;
  int currentColumn;
  int pos;
  int bpos;
  struct BufferPos *next;
  struct BufferPos *prev;
};

struct Document {
  const char *savecache;
  const char *title;
  const char *baseTarget;
  struct Url *baseURL;
  short width;
  short height;
  struct Line *firstLine;
  struct Line *topLine;
  struct Line *currentLine;
  struct Line *lastLine;
  int allLine;
  int currentColumn;
  short cursorX;
  short cursorY;
  int pos;
  int visualpos;
  short rootX;
  short rootY;
  short COLS;
  short LINES;
  struct AnchorList *href;
  struct AnchorList *name;
  struct AnchorList *img;
  struct AnchorList *formitem;
  struct LinkList *linklist;
  struct FormList *formlist;
  struct MapList *maplist;
  struct HmarkerList *hmarklist;
  struct HmarkerList *imarklist;
  struct BufferPos *undo;
};

struct Document *newDocument(int width);

#define TOP_LINENUMBER(doc) (doc->topLine ? doc->topLine->linenumber : 1)
#define CUR_LINENUMBER(doc)                                                    \
  (doc->currentLine ? doc->currentLine->linenumber : 1)

void addnewline(struct Document *doc, char *line, Lineprop *prop, int pos,
                int width, int nlines);
struct Line *currentLineSkip(struct Line *line, int offset, int last);
struct Line *lineSkip(struct Document *doc, struct Line *line, int offset,
                      int last);
void gotoLine(struct Document *doc, int n);
void arrangeCursor(struct Document *doc);
void cursorUp0(struct Document *doc, int n);
void cursorUp(struct Document *doc, int n);
void cursorDown0(struct Document *doc, int n);
void cursorDown(struct Document *doc, int n);
void cursorUpDown(struct Document *doc, int n);
void cursorRight(struct Document *doc, int n);
void cursorLeft(struct Document *doc, int n);
void cursorHome(struct Document *doc);
void arrangeLine(struct Document *doc);
void cursorXY(struct Document *doc, int x, int y);
int columnSkip(struct Document *doc, int offset);
void restorePosition(struct Document *doc, struct Document *orig);
int writeBufferCache(struct Document *doc);
int readBufferCache(struct Document *doc);
void tmpClearBuffer(struct Document *doc);
void copyBuffer(struct Document *a, const struct Document *b);
void COPY_BUFROOT(struct Document *dstbuf, const struct Document *srcbuf);
void COPY_BUFPOSITION(struct Document *dstbuf, const struct Document *srcbuf);
struct HmarkerList *putHmarker(struct HmarkerList *ml, int line, int pos,
                               int seq);
int currentLn(struct Document *doc);
