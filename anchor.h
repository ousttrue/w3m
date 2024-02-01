#pragma once

extern bool MarkAllPages;
extern int PagerMax;

struct BufferPoint {
  int line;
  int pos;
  int invalid;
};

struct Anchor {
  char *url;
  char *target;
  char *referer;
  char *title;
  unsigned char accesskey;
  BufferPoint start;
  BufferPoint end;
  int hseq;
  char slave;
  short y;
  short rows;
};

struct AnchorList {
  Anchor *anchors;
  int nanchor;
  int anchormax;
  int acache;
};

struct HmarkerList {
  BufferPoint *marks;
  int nmark;
  int markmax;
  int prevhseq;
};

AnchorList *putAnchor(AnchorList *al, char *url, char *target,
                      Anchor **anchor_return, char *referer, char *title,
                      unsigned char key, int line, int pos);
int onAnchor(Anchor *a, int line, int pos);
Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
Anchor *searchAnchor(AnchorList *al, char *str);
Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line, int pos,
                         int shift);
struct Buffer;
const char *reAnchor(Buffer *buf, const char *re);
