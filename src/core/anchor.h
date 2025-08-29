#pragma once

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct _anchor {
    char* url;
    char* target;
    char* referer;
    char* title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    struct _image* image;
} Anchor;

typedef struct _anchorList {
    Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
} AnchorList;
