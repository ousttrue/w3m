#pragma once
#include "url.h"
#include "geometry.h"
#include <wc.h>

extern wc_ces DocumentCharset;

struct Document {
    wc_ces charset;
    struct Url* baseURL;
    const char* baseTarget;
    const char* title;

    struct LineList* firstLine;
    int allLine;
    int topLineIndex;
    // cursor y in document
    int currentLineIndex;

    int cols;
    // cursor x in document
    int currentColumn;
    int pos;
    int visualpos;

    struct AnchorList* href;
    struct AnchorList* name;
    struct AnchorList* img;
    struct AnchorList* formitem;
    struct Form* formlist;
    struct LinkList* linklist;
    struct MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;
};

inline static void COPY_DOCUMENT_POSITION(struct Document* dst, struct Document* src)
{
    dst->topLineIndex = src->topLineIndex;
    dst->currentLineIndex = src->currentLineIndex;
    dst->pos = src->pos;
    dst->visualpos = src->visualpos;
    dst->currentColumn = src->currentColumn;
}

struct HtmlTagParsed;

struct LineList* getLine(struct Document* doc, int i);
struct LineList* lastLine(struct Document* doc);
inline static struct LineList* currentLine(struct Document* doc)
{
    return getLine(doc, doc->currentLineIndex);
}
inline static struct LineList* topLine(struct Document* doc)
{
    return getLine(doc, doc->topLineIndex);
}

struct Anchor* registerHref(struct Document* doc, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key,
    struct BufferPoint bp);
struct Anchor* registerName(struct Document* doc, const char* url, struct BufferPoint bp);
struct Anchor* registerForm(struct Document* doc, struct Form* flist, struct HtmlTagParsed* tag,
    struct BufferPoint bp);
struct Anchor* registerImg(struct Document* doc, const char* url, const char* title, struct BufferPoint bp);
void addMultirowsForm(struct Document* doc, struct AnchorList* al);
void addMultirowsImg(struct Document* doc, struct AnchorList* al);
