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

struct AnchorList {
    struct Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
};

struct HmarkerList {
    struct BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
};

struct Buffer;
struct Form;
struct HtmlTagParsed;

struct AnchorList* putAnchor(struct AnchorList* al, const char* url, const char* target,
    struct Anchor** anchor_return, const char* referer,
    const char* title, unsigned char key, int line,
    int pos);
struct Anchor* registerHref(struct Buffer* buf, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key,
    int line, int pos);
struct Anchor* registerName(struct Buffer* buf, const char* url, int line, int pos);
struct Anchor* registerImg(struct Buffer* buf, const char* url, const char* title, int line,
    int pos);
struct Anchor* registerForm(struct Buffer* buf, struct Form* flist,
    struct HtmlTagParsed* tag, int line, int pos);
int onAnchor(struct Anchor* a, int line, int pos);
struct Anchor* retrieveAnchor(struct AnchorList* al, int line, int pos);
struct Anchor* retrieveCurrentAnchor(struct Buffer* buf);
struct Anchor* retrieveCurrentImg(struct Buffer* buf);
struct Anchor* retrieveCurrentForm(struct Buffer* buf);
struct Anchor* searchAnchor(struct AnchorList* al, const char* str);
struct Anchor* searchURLLabel(struct Buffer* buf, const char* url);
void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos);
const char* reAnchor(struct Buffer* buf, const char* re);
void addMultirowsForm(struct Buffer* buf, struct AnchorList* al);
struct Anchor* closest_next_anchor(struct AnchorList* a, struct Anchor* an, int x, int y);
struct Anchor* closest_prev_anchor(struct AnchorList* a, struct Anchor* an, int x, int y);
void addMultirowsImg(struct Buffer* buf, struct AnchorList* al);
struct HmarkerList* putHmarker(struct HmarkerList* ml, int line, int pos, int seq);
void shiftAnchorPosition(struct AnchorList* a, struct HmarkerList* hl, int line,
    int pos, int shift);
const char* getAnchorText(struct Buffer* buf, struct AnchorList* al, struct Anchor* a);
struct Buffer* link_list_panel(struct Buffer* buf);
