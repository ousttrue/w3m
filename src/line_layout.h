#pragma once
#include "line.h"

extern int nextpage_topline;

struct Line;
struct BufferPos;
struct LineLayout {
  LineLayout();

  // always reshape new buffers to mark URLs
  bool need_reshape = true;

  //
  // lines
  //
  Line *firstLine = nullptr;
  Line *lastLine = nullptr;
  // scroll position
  Line *topLine = nullptr;
  int currentColumn = 0;
  // cursor position
  Line *currentLine = nullptr;
  int allLine = 0;

  void clearBuffer() {
    firstLine = topLine = currentLine = lastLine = nullptr;
    allLine = 0;
  }
  void addnewline(const char *line, Lineprop *prop, int byteLen, int breakWidth,
                  int realLinenum);
  int TOP_LINENUMBER() const { return topLine ? topLine->linenumber : 1; }
  int CUR_LINENUMBER() const {
    return currentLine ? currentLine->linenumber : 1;
  }
  void pushLine(Line *l) {
    if (!this->lastLine || this->lastLine == this->currentLine) {
      this->lastLine = l;
    }
    this->currentLine = l;
    if (!this->firstLine) {
      this->firstLine = l;
    }
  }
  Line *lineSkip(Line *line, int offset, int last);
  void arrangeLine();
  void cursorUpDown(int n);
  void gotoLine(int n);
  void cursorUp0(int n);
  void cursorUp(int n);
  void cursorDown0(int n);
  void cursorDown(int n);

  int columnSkip(int offset);
  void arrangeCursor();
  void cursorRight(int n);
  void cursorLeft(int n);
  void cursorHome();
  void cursorXY(int x, int y);
  void restorePosition(const LineLayout &orig);

  //
  // viewport
  //
  short rootX = 0;
  short rootY = 0;
  short COLS = 0;
  short LINES = 0;
  // COLS/LINES と width height の違いは？
  short width = 0;
  short height = 0;
  short cursorX = 0;
  short cursorY = 0;

  int pos = 0;
  int visualpos = 0;

  BufferPos *undo = nullptr;

  int AbsCursorX() const { return cursorX + rootX; }
  int AbsCursorY() const { return cursorY + rootY; }

  inline void COPY_BUFROOT_FROM(const LineLayout &srcbuf) {
    this->rootX = srcbuf.rootX;
    this->rootY = srcbuf.rootY;
    this->COLS = srcbuf.COLS;
    this->LINES = srcbuf.LINES;
  }

  void COPY_BUFPOSITION_FROM(const LineLayout &srcbuf) {
    this->topLine = srcbuf.topLine;
    this->currentLine = srcbuf.currentLine;
    this->pos = srcbuf.pos;
    this->cursorX = srcbuf.cursorX;
    this->cursorY = srcbuf.cursorY;
    this->visualpos = srcbuf.visualpos;
    this->currentColumn = srcbuf.currentColumn;
  }

  //
  // Anchors
  //
  struct AnchorList *href = nullptr;
  AnchorList *name = nullptr;
  AnchorList *img = nullptr;
  AnchorList *formitem = nullptr;
  struct LinkList *linklist = nullptr;
  struct FormList *formlist = nullptr;
  struct MapList *maplist = nullptr;
  struct HmarkerList *hmarklist = nullptr;
  HmarkerList *imarklist = nullptr;
  struct FormItemList *form_submit = nullptr;
  struct Anchor *submit = nullptr;
};
