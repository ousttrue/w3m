#include "hmarker.h"
#include "alloc.h"
#include <string.h>

#define FIRST_MARKER_SIZE 30

struct HmarkerList*
hm_put(struct HmarkerList* ml, struct BufferPoint bp, int seq)
{
    if (ml == NULL) {
        ml = New(struct HmarkerList);
        ml->marks = NULL;
        ml->nmark = 0;
        ml->markmax = 0;
        ml->prevhseq = -1;
    }
    if (ml->markmax == 0) {
        ml->markmax = FIRST_MARKER_SIZE;
        ml->marks = NewAtom_N(struct BufferPoint, ml->markmax);
        memset(ml->marks, 0, sizeof(struct BufferPoint) * ml->markmax);
    }
    if (seq + 1 > ml->nmark)
        ml->nmark = seq + 1;
    if (ml->nmark >= ml->markmax) {
        ml->markmax = ml->nmark * 2;
        ml->marks = New_Reuse(struct BufferPoint, ml->marks, ml->markmax);
    }
    ml->marks[seq] = bp;
    ml->marks[seq].invalid = 0;
    return ml;
}
