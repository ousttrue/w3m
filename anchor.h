#pragma once
#include "Str.h"
#include "geometry.h"
#include <stdbool.h>

struct Anchor {
    const char* url;
    const char* target;
    const char* referer;
    const char* title;
    unsigned char accesskey;
    struct BufferPoint start;
    struct BufferPoint end;
    int hseq;
    bool slave;
    short y;
    short rows;
    struct Image* image;
};
inline static int onAnchor(struct Anchor* a, struct BufferPoint bp)
{
    if (bpcmp(bp, a->start) < 0)
        return -1;
    if (bpcmp(a->end, bp) <= 0)
        return 1;
    return 0;
}

struct Document;
struct HtmlBuilder;
struct FormList;
struct Line;
struct Url;

struct HtmlTag;
struct Anchor* registerForm(struct HtmlBuilder* hb, struct Document* doc, struct BufferPoint bp,
    struct FormList* flist, struct HtmlTag* tag);

const char* reAnchor(struct Url* base_url, struct Document* doc, const char* re);
void reAnchorWord(struct Url* base_url, struct Document* doc, struct Line* l, int spos, int epos);

