#pragma once

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
  struct BufferPoint start;
  struct BufferPoint end;
  int hseq;
  char slave;
  short y;
  short rows;
};

struct AnchorList {
  struct Anchor *anchors;
  int nanchor;
  int anchormax;
  int acache;
};

struct HmarkerList {
  struct BufferPoint *marks;
  int nmark;
  int markmax;
  int prevhseq;
};

struct Document;
extern struct AnchorList *putAnchor(struct AnchorList *al, const char *url,
                                    const char *target,
                                    struct Anchor **anchor_return,
                                    const char *referer, const char *title,
                                    unsigned char key, int line, int pos);

struct Anchor *registerHref(struct Document *doc, const char *url,
                            const char *target, const char *referer,
                            const char *title, unsigned char key, int line,
                            int pos);
struct Anchor *registerName(struct Document *doc, char *url, int line, int pos);
struct Anchor *registerImg(struct Document *doc, char *url, char *title,
                           int line, int pos);
struct Anchor *searchURLLabel(struct Document *doc, const char *url);
struct Anchor *retrieveAnchor(struct AnchorList *al, int line, int pos);
struct Anchor *retrieveCurrentAnchor(struct Document *doc);
struct Anchor *retrieveCurrentImg(struct Document *doc);
struct Anchor *retrieveCurrentForm(struct Document *doc);
struct Anchor *searchAnchor(struct AnchorList *al, const char *str);
struct Anchor;
extern int onAnchor(struct Anchor *a, int line, int pos);
struct Line;
struct Buffer;
extern void reAnchorWord(struct Buffer *buf, struct Line *l, int spos,
                         int epos);
extern char *reAnchor(struct Buffer *buf, char *re);
struct AnchorList;
extern void addMultirowsForm(struct Buffer *buf, struct AnchorList *al);
extern struct Anchor *closest_next_anchor(struct AnchorList *a,
                                          struct Anchor *an, int x, int y);
extern struct Anchor *closest_prev_anchor(struct AnchorList *a,
                                          struct Anchor *an, int x, int y);
struct HmarkerList;
extern void shiftAnchorPosition(struct AnchorList *a, struct HmarkerList *hl,
                                int line, int pos, int shift);
extern char *getAnchorText(struct Buffer *buf, struct AnchorList *al,
                           struct Anchor *a);


