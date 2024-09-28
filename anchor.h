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
