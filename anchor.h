#pragma once
#include <sys/types.h>

typedef struct _ImageCache {
    char* url;
    struct _ParsedURL* current;
    char* file;
    char* touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
} ImageCache;

typedef struct _image {
    char* url;
    char* ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    char* map;
    char ismap;
    int touch;
    ImageCache* cache;
} Image;

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct _Anchor {
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
    Image* image;
} Anchor;

typedef struct _AnchorList {
    Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
} AnchorList;

typedef struct _HmarkerList {
    BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;
