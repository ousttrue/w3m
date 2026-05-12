#ifndef W3M_ANCHOR_H_
#define W3M_ANCHOR_H_

#include "config.h"
#include "image.h"

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct {
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
#ifdef USE_IMAGE
    Image *image;
#endif
} Anchor;

typedef struct {
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
#endif
