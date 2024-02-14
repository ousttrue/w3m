#pragma once
#include "line.h"
#include "url.h"
#include <string>
#include <optional>

extern int nextpage_topline;

struct Line;
struct BufferPos;
struct HtmlTag;
struct LineLayout {
  LineLayout();
  LineLayout(int width);

  std::string title;
  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  int refresh_interval = 0;

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
  int currentLn() const {
    if (currentLine) {
      return currentLine->linenumber + 1;
    } else {
      return 1;
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
  void gotoRealLine(int n);
  void nscroll(int n);

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

  void nextY(int d, int n);
  void nextX(int d, int dy, int n);
  void _prevA(int visited, std::optional<Url> baseUrl, int n);
  void _nextA(int visited, std::optional<Url> baseUrl, int n);
  void _movL(int n, int m);
  void _movD(int n, int m);
  void _movU(int n, int m);
  void _movR(int n, int m);
  int prev_nonnull_line(Line *line);
  int next_nonnull_line(Line *line);
  void _goLine(const char *l, int prec_num);
  void shiftvisualpos(int shift);
  std::string getCurWord(int *spos, int *epos) const;
  std::string getCurWord() const {
    int s, e;
    return getCurWord(&s, &e);
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

  Anchor *registerHref(const char *url, const char *target, const char *referer,
                       const char *title, unsigned char key, int line, int pos);
  Anchor *registerName(const char *url, int line, int pos);
  Anchor *registerImg(const char *url, const char *title, int line, int pos);
  Anchor *registerForm(FormList *flist, HtmlTag *tag, int line, int pos);
  void addMultirowsForm(AnchorList *al);
  Anchor *searchURLLabel(const char *url);
};

inline void nextChar(int &s, Line *l) { s++; }
inline void prevChar(int &s, Line *l) { s--; }
