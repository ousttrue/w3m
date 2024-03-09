#pragma once
#include "rowcol.h"
#include "line_data.h"
#include "http_request.h"
#include "url.h"
#include <string>
#include <optional>

extern int nextpage_topline;
extern int FoldTextarea;

struct HtmlTag;

struct LineLayout {
  LineData data;

  LineLayout();
  LineLayout(int width);

  //
  // lines
  //
  Line *firstLine() { return data.firstLine(); }
  Line *lastLine() { return data.lastLine(); }
  const Line *lastLine() const { return data.lastLine(); }
  bool empty() const { return data.lines.empty(); }
  int linenumber(const Line *t) const { return data.linenumber(t); }
  int TOP_LINENUMBER() const { return _topLine; }
  int CUR_LINENUMBER() const { return linenumber(currentLine()); }
  bool isNull(Line *l) const { return linenumber(l) < 0; }
  bool hasNext(const Line *l) const { return linenumber(++l) >= 0; }
  bool hasPrev(const Line *l) const { return linenumber(--l) >= 0; }

  // scroll position
  int _topLine = 0;
  Line *topLine() {
    if (_topLine < 0 || _topLine >= (int)data.lines.size()) {
      return nullptr;
    }
    return &data.lines[_topLine];
  }

  // cursor position
  int currentColumn = 0;
  Line *currentLine() {
    auto _currentLine = _topLine + cursor.row;
    if (_currentLine < 0 || _currentLine >= (int)data.lines.size()) {
      return nullptr;
    }
    return &data.lines[_currentLine];
  }
  const Line *currentLine() const {
    auto _currentLine = _topLine + cursor.row;
    if (_currentLine < 0 || _currentLine >= (int)data.lines.size()) {
      return nullptr;
    }
    return &data.lines[_currentLine];
  }

  bool check_url = false;
  void chkURLBuffer();
  void reshape(int width, const LineLayout &sbuf);

  void clearBuffer();

  int lineSkip(const Line *line, int offset) const {
    auto l = currentLineSkip(line, offset);
    if (!nextpage_topline) {
      for (int i = LINES - 1 - (linenumber(lastLine()) - l);
           i > 0 && hasPrev(&data.lines[l]); i--, --l)
        ;
    }
    return l;
  }

  int currentLineSkip(const Line *l, int offset) const {
    if (offset > 0)
      for (int i = 0; i < offset && hasNext(l); i++, ++l)
        ;
    else if (offset < 0)
      for (int i = 0; i < -offset && hasPrev(l); i++, --l)
        ;
    return linenumber(l);
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

  //
  // viewport
  //
  short COLS = 0;
  short LINES = 0;
  // COLS/LINES と width height の違いは？
  short width = 0;
  short height = 0;
  RowCol cursor = {0, 0};

  int pos = 0;
  int visualpos = 0;

  inline void COPY_BUFROOT_FROM(const LineLayout &srcbuf) {
    this->COLS = srcbuf.COLS;
    this->LINES = srcbuf.LINES;
  }

  void COPY_BUFPOSITION_FROM(const LineLayout &srcbuf) {
    this->_topLine = srcbuf._topLine;
    this->pos = srcbuf.pos;
    this->cursor = srcbuf.cursor;
    this->visualpos = srcbuf.visualpos;
    this->currentColumn = srcbuf.currentColumn;
  }

  void nextY(int d, int n);
  void nextX(int d, int dy, int n);
  void _prevA(std::optional<Url> baseUrl, int n);
  void _nextA(std::optional<Url> baseUrl, int n);
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
  FormAnchor *retrieveCurrentForm();
  void formResetBuffer(const AnchorList<FormAnchor> *formitem);
  void formUpdateBuffer(FormAnchor *a);
};

inline void nextChar(int &s, const Line *l) { s++; }
inline void prevChar(int &s, const Line *l) { s--; }
