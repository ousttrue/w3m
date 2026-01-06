#pragma once
#include "geometry.h"

struct HmarkerList {
    struct BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
};

struct HmarkerList* hm_put(struct HmarkerList* ml, struct BufferPoint bp, int seq);
