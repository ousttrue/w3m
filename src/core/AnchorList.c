#include "AnchorList.h"
#include "Anchor.h"
#include "geometry.h"
#include "alloc.h"
#include <string.h>

#define FIRST_ANCHOR_SIZE 30

#define bpcmp(a, b) \
    (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

struct AnchorList*
putAnchor(struct AnchorList* al, struct Anchor** anchor_return, struct BufferPoint bp)
{
    int n, i, j;
    if (al == NULL) {
        al = New(struct AnchorList);
        al->anchors = NULL;
        al->nanchor = al->anchormax = 0;
        al->acache = -1;
    }
    if (al->anchormax == 0) {
        /* first time; allocate struct Anchor buffer */
        al->anchors = New_N(struct Anchor, FIRST_ANCHOR_SIZE);
        al->anchormax = FIRST_ANCHOR_SIZE;
    }
    if (al->nanchor == al->anchormax) { /* need realloc */
        al->anchormax *= 2;
        al->anchors = New_Reuse(struct Anchor, al->anchors, al->anchormax);
    }

    n = al->nanchor;
    if (!n || bpcmp(al->anchors[n - 1].start, bp) < 0)
        i = n;
    else
        for (i = 0; i < n; i++) {
            if (bpcmp(al->anchors[i].start, bp) >= 0) {
                for (j = n; j > i; j--)
                    al->anchors[j] = al->anchors[j - 1];
                break;
            }
        }

    struct Anchor* a = &al->anchors[i];
    a->start = bp;
    a->end = bp;
    al->nanchor++;
    if (anchor_return)
        *anchor_return = a;
    return al;
}

void reseq_anchor0(struct AnchorList* al, short* seqmap)
{
    int i;
    struct Anchor* a;

    if (!al)
        return;

    for (i = 0; i < al->nanchor; i++) {
        a = &al->anchors[i];
        if (a->hseq >= 0) {
            a->hseq = seqmap[a->hseq];
        }
    }
}

struct Anchor* retrieveAnchor(struct AnchorList* al, int line, int pos)
{
    if (al == NULL || al->nanchor == 0)
        return NULL;

    if (al->acache < 0 || al->acache >= al->nanchor)
        al->acache = 0;

    for (size_t b = 0, e = al->nanchor - 1; b <= e; al->acache = (b + e) / 2) {
        struct Anchor* a = &al->anchors[al->acache];
        int cmp = onAnchor(a, line, pos);
        if (cmp == 0)
            return a;
        else if (cmp > 0)
            b = al->acache + 1;
        else if (al->acache == 0)
            return NULL;
        else
            e = al->acache - 1;
    }
    return NULL;
}

struct Anchor*
closest_next_anchor(struct AnchorList* a, struct Anchor* an, int x, int y)
{
    int i;

    if (a == NULL || a->nanchor == 0)
        return an;
    for (i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].start.line > y || (a->anchors[i].start.line == y && a->anchors[i].start.pos > x)) {
            if (an == NULL || an->start.line > a->anchors[i].start.line || (an->start.line == a->anchors[i].start.line && an->start.pos > a->anchors[i].start.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}

struct Anchor*
closest_prev_anchor(struct AnchorList* a, struct Anchor* an, int x, int y)
{
    int i;

    if (a == NULL || a->nanchor == 0)
        return an;
    for (i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].end.line < y || (a->anchors[i].end.line == y && a->anchors[i].end.pos <= x)) {
            if (an == NULL || an->end.line < a->anchors[i].end.line || (an->end.line == a->anchors[i].end.line && an->end.pos < a->anchors[i].end.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}

#define FIRST_MARKER_SIZE 30
struct HmarkerList*
putHmarker(struct HmarkerList* ml, int line, int pos, int seq)
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
    ml->marks[seq].line = line;
    ml->marks[seq].pos = pos;
    ml->marks[seq].invalid = 0;
    return ml;
}

void shiftAnchorPosition(struct AnchorList* al, struct HmarkerList* hl, int line, int pos,
    int shift)
{
    struct Anchor* a;
    size_t b, e, s = 0;
    int cmp;

    if (al == NULL || al->nanchor == 0)
        return;

    s = al->nanchor / 2;
    for (b = 0, e = al->nanchor - 1; b <= e; s = (b + e + 1) / 2) {
        a = &al->anchors[s];
        cmp = onAnchor(a, line, pos);
        if (cmp == 0)
            break;
        else if (cmp > 0)
            b = s + 1;
        else if (s == 0)
            break;
        else
            e = s - 1;
    }
    for (; s < al->nanchor; s++) {
        a = &al->anchors[s];
        if (a->start.line > line)
            break;
        if (a->start.pos > pos) {
            a->start.pos += shift;
            if (hl && hl->marks && a->hseq >= 0 && hl->marks[a->hseq].line == line)
                hl->marks[a->hseq].pos = a->start.pos;
        }
        if (a->end.pos >= pos)
            a->end.pos += shift;
    }
}
