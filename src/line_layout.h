#pragma once
#include "line_data.h"
#include "http_request.h"
#include "url.h"
#include <string>
#include <optional>

extern int nextpage_topline;

struct BufferPos {
  long top_linenumber;
  long cur_linenumber;
  int currentColumn;
  int pos;
  int bpos;
  BufferPos *next;
  BufferPos *prev;
};

struct Line;
struct BufferPos;
struct HtmlTag;
struct AnchorList;
struct HmarkerList;

struct LineLayout {
  LineData data;

  LineLayout();
  LineLayout(int width);

  //
  // lines
  //
  Line *firstLine() { return data.firstLine(); }
  Line *lastLine() { return data.lastLine(); }
  bool empty() const { return data.lines.empty(); }
  int linenumber(Line *t) const { return data.linenumber(t); }
  int TOP_LINENUMBER() const { return linenumber(topLine); }
  int CUR_LINENUMBER() const { return linenumber(currentLine); }
  bool isNull(Line *l) const { return linenumber(l) < 0; }
  bool hasNext(Line *l) const { return linenumber(++l) >= 0; }
  bool hasPrev(Line *l) const { return linenumber(--l) >= 0; }

  // scroll position
  Line *topLine = nullptr;
  int currentColumn = 0;
  // cursor position
  Line *currentLine = nullptr;
  bool check_url = false;
  void chkURLBuffer();
  void reshape(int width, const LineLayout &sbuf);

  void clearBuffer();

  Line *lineSkip(Line *line, int offset);
  Line *currentLineSkip(Line *l, int offset) {
    if (offset > 0)
      for (int i = 0; i < offset && hasNext(l); i++, ++l)
        ;
    else if (offset < 0)
      for (int i = 0; i < -offset && hasPrev(l); i++, --l)
        ;
    return l;
  }
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
  void nscroll(int n);

  void save_buffer_position();
  void resetPos(BufferPos *b);
  void undoPos(int n);
  void redoPos(int n);

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
  void _prevA(bool visited, std::optional<Url> baseUrl, int n);
  void _nextA(bool visited, std::optional<Url> baseUrl, int n);
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

  Anchor *retrieveCurrentAnchor();
  Anchor *retrieveCurrentImg();
  Anchor *retrieveCurrentForm();
};

inline void nextChar(int &s, Line *l) { s++; }
inline void prevChar(int &s, Line *l) { s--; }
