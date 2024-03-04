#pragma once
#include "line.h"
#include "http_request.h"
#include "html/anchor.h"
#include "url.h"
#include <string>
#include <optional>
#include <memory>

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

using AnchorFunc = Anchor *(*)(struct LineData *, const char *, const char *,
                               int, int);
struct LineData {
  std::string title;
  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  int refresh_interval = 0;
  std::vector<Line> lines;
  std::shared_ptr<AnchorList> _href;
  std::shared_ptr<AnchorList> _name;
  std::shared_ptr<AnchorList> _img;
  std::shared_ptr<AnchorList> _formitem;
  std::shared_ptr<HmarkerList> _hmarklist;
  std::shared_ptr<HmarkerList> _imarklist;
  struct LinkList *linklist = nullptr;
  struct FormList *formlist = nullptr;
  struct MapList *maplist = nullptr;
  struct FormItemList *form_submit = nullptr;
  struct Anchor *submit = nullptr;

  LineData();
  Line *firstLine() { return lines.size() ? &lines.front() : nullptr; }
  Line *lastLine() { return lines.size() ? &lines.back() : nullptr; }
  int linenumber(Line *t) const {
    for (int i = 0; i < (int)lines.size(); ++i) {
      if (&lines[i] == t) {
        return i;
      }
    }
    assert(false);
    return -1;
  }
  void clear() {
    lines.clear();
    this->_href->clear();
    this->_name->clear();
    this->_img->clear();
    this->_formitem->clear();
    this->formlist = nullptr;
    this->linklist = nullptr;
    this->maplist = nullptr;
    this->_hmarklist->clear();
    this->_imarklist->clear();
  }
  void addnewline(const char *line, Lineprop *prop, int byteLen);
  std::shared_ptr<AnchorList> href() const { return _href; }
  std::shared_ptr<AnchorList> name() const { return _name; }
  std::shared_ptr<AnchorList> img() const { return _img; }
  std::shared_ptr<AnchorList> formitem() const { return _formitem; }
  std::shared_ptr<HmarkerList> hmarklist() const { return _hmarklist; }
  std::shared_ptr<HmarkerList> imarklist() const { return _imarklist; }

  Anchor *registerName(const char *url, int line, int pos) {
    return this->name()->putAnchor(url, NULL, {}, NULL, '\0', line, pos);
  }
  Anchor *registerImg(const char *url, const char *title, int line, int pos) {
    return this->img()->putAnchor(url, NULL, {}, title, '\0', line, pos);
  }
  Anchor *registerForm(class HtmlParser *parser, FormList *flist, HtmlTag *tag,
                       int line, int pos);
  void addMultirowsForm(AnchorList *al);
  void reseq_anchor();
  const char *reAnchorPos(Line *l, const char *p1, const char *p2,
                          AnchorFunc anchorproc);
  const char *reAnchorAny(Line *topLine, const char *re, AnchorFunc anchorproc);
  const char *reAnchor(Line *topLine, const char *re);
  const char *reAnchorWord(Line *l, int spos, int epos);
  const char *getAnchorText(AnchorList *al, Anchor *a);

  void addLink(struct HtmlTag *tag);
};

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

  Anchor *retrieveCurrentAnchor() {
    if (!this->currentLine || !this->data.href())
      return NULL;
    return this->data.href()->retrieveAnchor(linenumber(currentLine),
                                             this->pos);
  }

  Anchor *retrieveCurrentImg() {
    if (!this->currentLine || !this->data.img())
      return NULL;
    return this->data.img()->retrieveAnchor(linenumber(currentLine), this->pos);
  }

  Anchor *retrieveCurrentForm() {
    if (!this->currentLine || !this->data.formitem())
      return NULL;
    return this->data.formitem()->retrieveAnchor(linenumber(currentLine),
                                                 this->pos);
  }
};

inline void nextChar(int &s, Line *l) { s++; }
inline void prevChar(int &s, Line *l) { s--; }
