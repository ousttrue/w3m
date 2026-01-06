#pragma once
#include "geometry.h"

struct AnchorList {
    struct Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
};

struct Anchor* al_put(struct AnchorList* al,
    const char* url, const char* target, const char* referer,
    const char* title, unsigned char key, struct BufferPoint bp);
struct Anchor* al_retrieve(struct AnchorList* al, struct BufferPoint bp);
struct Anchor* al_find(struct AnchorList* al, const char* str);
struct Anchor* al_closestNext(struct AnchorList* a, struct Anchor* an, struct BufferPoint bp);
struct Anchor* al_closestPrev(struct AnchorList* a, struct Anchor* an, struct BufferPoint bp);
