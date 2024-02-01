#pragma once

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

AnchorList *putAnchor(AnchorList *al, const char *url, const char *target,
                      Anchor **anchor_return, const char *referer,
                      const char *title, unsigned char key, int line, int pos);
int onAnchor(Anchor *a, int line, int pos);
Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
Anchor *searchAnchor(AnchorList *al, const char *str);
Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line, int pos,
                         int shift);
struct Buffer;
const char *reAnchor(Buffer *buf, const char *re);
Anchor *registerHref(Buffer *buf, const char *url, const char *target,
                     const char *referer, const char *title, unsigned char key,
                     int line, int pos);
Anchor *registerName(Buffer *buf, const char *url, int line, int pos);
Anchor *registerImg(Buffer *buf, const char *url, const char *title, int line,
                    int pos);
struct FormList;
Anchor *registerForm(Buffer *buf, FormList *flist, struct parsed_tag *tag,
                     int line, int pos);

Anchor *retrieveCurrentAnchor(Buffer *buf);
Anchor *retrieveCurrentImg(Buffer *buf);
Anchor *retrieveCurrentForm(Buffer *buf);
Anchor *searchURLLabel(Buffer *buf, const char *url);
struct Line;
void reAnchorWord(Buffer *buf, Line *l, int spos, int epos);
struct AnchorList;
void addMultirowsForm(Buffer *buf, AnchorList *al);

const char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a);
Buffer *link_list_panel(Buffer *buf);
