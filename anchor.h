#pragma once

typedef struct {
  int line;
  int pos;
  int invalid;
} BufferPoint;

typedef struct Anchor {
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
} Anchor;

typedef struct AnchorList {
  Anchor *anchors;
  int nanchor;
  int anchormax;
  int acache;
} AnchorList;

typedef struct {
  BufferPoint *marks;
  int nmark;
  int markmax;
  int prevhseq;
} HmarkerList;
