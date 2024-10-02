#pragma once

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
extern struct AnchorList *putAnchor(struct AnchorList *al, char *url,
                                    char *target, struct Anchor **anchor_return,
                                    char *referer, char *title,
                                    unsigned char key, int line, int pos);

struct Anchor *registerHref(struct Document *doc, char *url, char *target,
                            char *referer, char *title, unsigned char key,
                            int line, int pos);
struct Anchor *registerName(struct Document *doc, char *url, int line, int pos);
struct Anchor *registerImg(struct Document *doc, char *url, char *title,
                           int line, int pos);
struct Anchor *searchURLLabel(struct Document *doc, const char *url);
struct Anchor *retrieveAnchor(struct AnchorList *al, int line, int pos);
struct Anchor *retrieveCurrentAnchor(struct Document *doc);
struct Anchor *retrieveCurrentImg(struct Document *doc);
struct Anchor *retrieveCurrentForm(struct Document *doc);
struct Anchor *searchAnchor(struct AnchorList *al, const char *str);
