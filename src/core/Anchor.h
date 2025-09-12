#pragma once
#include "line.h"
#include "geometry.h"

extern int MarkAllPages;

struct Anchor {
    const char* url;
    const char* target;
    const char* referer;
    const char* title;
    unsigned char accesskey;
    struct BufferPoint start;
    struct BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    struct Image* image;
};

void initAnchor(struct Anchor* a, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key);
int onAnchor(struct Anchor* a, struct BufferPoint bp);
