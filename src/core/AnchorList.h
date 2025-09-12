#pragma once
#include "geometry.h"

struct AnchorList {
    struct Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
};

struct AnchorList* putAnchor(struct AnchorList* al, struct Anchor** anchor_return, struct BufferPoint bp);
void reseq_anchor0(struct AnchorList* al, short* seqmap);
struct Anchor* retrieveAnchor(struct AnchorList* al, struct BufferPoint bp);
struct Anchor* searchAnchor(struct AnchorList* al, const char* str);
struct Anchor* closest_next_anchor(struct AnchorList* a, struct Anchor* an, int x, int y);
struct Anchor* closest_prev_anchor(struct AnchorList* a, struct Anchor* an, int x, int y);

struct HmarkerList {
    struct BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
};

struct HmarkerList* putHmarker(struct HmarkerList* ml, int line, int pos, int seq);
void shiftAnchorPosition(struct AnchorList* a, struct HmarkerList* hl, struct BufferPoint bp, int shift);

