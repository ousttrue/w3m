#pragma once
#include <wc.h>
#include "url.h"
#include "geometry.h"

struct Document {
    wc_ces charset;
    struct Url* baseURL;
    const char* baseTarget;

    struct LineList* firstLine;
    int allLine;
    struct AnchorList* href;
    struct AnchorList* name;
    struct AnchorList* img;
    struct AnchorList* formitem;
    struct Form* formlist;
    struct LinkList* linklist;
    struct _MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;
};

struct HtmlTagParsed;

struct Anchor* registerHref(struct Document* doc, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key,
    struct BufferPoint bp);
struct Anchor* registerName(struct Document* doc, const char* url, struct BufferPoint bp);
struct Anchor* registerForm(struct Document* doc, struct Form* flist, struct HtmlTagParsed* tag,
    struct BufferPoint bp);
struct Anchor* registerImg(struct Document* doc, const char* url, const char* title, struct BufferPoint bp);
