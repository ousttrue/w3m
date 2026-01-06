#include "anchor_list.h"
#include "alloc.h"
#include <assert.h>
#include <string.h>

#define FIRST_ANCHOR_SIZE 30

struct Anchor*
al_put(struct AnchorList* al, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key, struct BufferPoint bp)
{
    assert(al);
    // if (al == NULL) {
    //     al = New(struct AnchorList);
    //     al->anchors = NULL;
    //     al->nanchor = al->anchormax = 0;
    //     al->acache = -1;
    // }
    if (al->anchormax == 0) {
        /* first time; allocate anchor buffer */
        al->anchors = New_N(struct Anchor, FIRST_ANCHOR_SIZE);
        al->anchormax = FIRST_ANCHOR_SIZE;
    }
    if (al->nanchor == al->anchormax) { /* need realloc */
        al->anchormax *= 2;
        al->anchors = New_Reuse(struct Anchor, al->anchors, al->anchormax);
    }

    int n = al->nanchor;
    int i;
    if (!n || bpcmp(al->anchors[n - 1].start, bp) < 0)
        i = n;
    else
        for (i = 0; i < n; i++) {
            if (bpcmp(al->anchors[i].start, bp) >= 0) {
                for (int j = n; j > i; j--)
                    al->anchors[j] = al->anchors[j - 1];
                break;
            }
        }

    struct Anchor* a;
    a = &al->anchors[i];
    *a = (struct Anchor) {
        .url = url,
        .target = target,
        .referer = referer,
        .title = title,
        .accesskey = key,
        .slave = false,
        .start = bp,
        .end = bp,
    };
    al->nanchor++;
    // if (anchor_return)
    //     *anchor_return = a;
    return a;
}

struct Anchor* al_retrieve(struct AnchorList* al, struct BufferPoint bp)
{
    if (al == NULL || al->nanchor == 0)
        return NULL;

    if (al->acache < 0 || al->acache >= al->nanchor)
        al->acache = 0;

    struct Anchor* a;
    size_t b, e;
    int cmp;
    for (b = 0, e = al->nanchor - 1; b <= e; al->acache = (b + e) / 2) {
        a = &al->anchors[al->acache];
        cmp = onAnchor(a, bp);
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

struct Anchor* al_find(struct AnchorList* al, const char* str)
{
    if (!al)
        return 0;
    for (int i = 0; i < al->nanchor; i++) {
        struct Anchor* a = &al->anchors[i];
        if (a->hseq < 0)
            continue;
        if (!strcmp(a->url, str))
            return a;
    }
    return 0;
}

struct Anchor*
al_closestNext(struct AnchorList* a, struct Anchor* an, struct BufferPoint bp)
{
    if (a == NULL || a->nanchor == 0)
        return an;
    for (int i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].start.line > bp.line || (a->anchors[i].start.line == bp.line && a->anchors[i].start.pos > bp.pos)) {
            if (an == NULL || an->start.line > a->anchors[i].start.line || (an->start.line == a->anchors[i].start.line && an->start.pos > a->anchors[i].start.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}

struct Anchor*
al_closestPrev(struct AnchorList* a, struct Anchor* an, struct BufferPoint bp)
{
    if (a == NULL || a->nanchor == 0)
        return an;
    for (int i = 0; i < a->nanchor; i++) {
        if (a->anchors[i].hseq < 0)
            continue;
        if (a->anchors[i].end.line < bp.line || (a->anchors[i].end.line == bp.line && a->anchors[i].end.pos <= bp.pos)) {
            if (an == NULL || an->end.line < a->anchors[i].end.line || (an->end.line == a->anchors[i].end.line && an->end.pos < a->anchors[i].end.pos))
                an = &a->anchors[i];
        }
    }
    return an;
}
