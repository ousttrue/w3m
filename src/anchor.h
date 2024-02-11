#pragma once
#include <gc_cpp.h>
#include <vector>

#define bpcmp(a, b)                                                            \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

extern bool MarkAllPages;
extern int PagerMax;

struct BufferPoint {
  int line;
  int pos;
  int invalid;
};

struct Anchor {
  const char *url;
  const char *target;
  const char *referer;
  const char *title;
  unsigned char accesskey;
  BufferPoint start;
  BufferPoint end;
  int hseq;
  char slave;
  short y;
  short rows;
};

struct AnchorList : public gc_cleanup {
  std::vector<Anchor> anchors;
  size_t size() const { return anchors.size(); }

  AnchorList() {}
  ~AnchorList() {}
  AnchorList(const AnchorList &) = delete;
  AnchorList &operator=(const AnchorList &) = delete;

  Anchor *retrieveAnchor(int line, int pos);

private:
  int acache = -1;
};

struct HmarkerList {
  BufferPoint *marks;
  int nmark;
  int markmax;
  int prevhseq;
};

int onAnchor(Anchor *a, int line, int pos);
Anchor *searchAnchor(AnchorList *al, const char *str);
Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line, int pos,
                         int shift);

struct LineLayout;
const char *reAnchor(LineLayout *layout, const char *re);
Anchor *retrieveCurrentAnchor(LineLayout *layout);
Anchor *retrieveCurrentImg(LineLayout *layout);
Anchor *retrieveCurrentForm(LineLayout *layout);
struct Line;
void reAnchorWord(LineLayout *layout, Line *l, int spos, int epos);
struct AnchorList;

const char *getAnchorText(LineLayout *layout, AnchorList *al, Anchor *a);

const char *html_quote(const char *str);
const char *html_unquote(const char *str);
int getescapechar(const char **s);
const char *getescapecmd(const char **s);
