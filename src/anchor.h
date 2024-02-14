#pragma once
#include <vector>
#include <string_view>

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

  int onAnchor(int line, int pos);
};

struct HmarkerList {
  std::vector<BufferPoint> marks;
  size_t size() const { return marks.size(); }
  void clear() { marks.clear(); }
  int prevhseq = -1;
  void putHmarker(int line, int pos, int seq);
};

struct AnchorList {
  std::vector<Anchor> anchors;
  size_t size() const { return anchors.size(); }
  void clear() { anchors.clear(); }

  AnchorList() {}
  ~AnchorList() {}
  AnchorList(const AnchorList &) = delete;
  AnchorList &operator=(const AnchorList &) = delete;

  Anchor *searchAnchor(std::string_view str);
  Anchor *retrieveAnchor(int line, int pos);
  Anchor *closest_next_anchor(Anchor *an, int x, int y);
  Anchor *closest_prev_anchor(Anchor *an, int x, int y);
  void shiftAnchorPosition(HmarkerList *hl, int line, int pos, int shift);
  void reseq_anchor0(short *seqmap);

private:
  int acache = -1;
};

const char *html_quote(const char *str);
const char *html_unquote(const char *str);
int getescapechar(const char **s);
const char *getescapecmd(const char **s);
