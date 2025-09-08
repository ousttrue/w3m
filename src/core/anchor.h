#pragma once
#include "line.h"

extern int MarkAllPages;

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct _anchor {
    const char* url;
    const char* target;
    const char* referer;
    const char* title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    struct Image* image;
} Anchor;

typedef struct _anchorList {
    Anchor* anchors;
    int nanchor;
    int anchormax;
    int acache;
} AnchorList;

typedef struct {
    BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
} HmarkerList;

struct _Buffer;
struct Form;
struct HtmlTagParsed;

AnchorList* putAnchor(AnchorList* al, const char* url, const char* target,
    Anchor** anchor_return, const char* referer,
    const char* title, unsigned char key, int line,
    int pos);
Anchor* registerHref(struct _Buffer* buf, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(struct _Buffer* buf, const char* url, int line, int pos);
Anchor* registerImg(struct _Buffer* buf, const char* url, const char* title, int line,
    int pos);
Anchor* registerForm(struct _Buffer* buf, struct Form* flist,
    struct HtmlTagParsed* tag, int line, int pos);
int onAnchor(Anchor* a, int line, int pos);
Anchor* retrieveAnchor(AnchorList* al, int line, int pos);
Anchor* retrieveCurrentAnchor(struct _Buffer* buf);
Anchor* retrieveCurrentImg(struct _Buffer* buf);
Anchor* retrieveCurrentForm(struct _Buffer* buf);
Anchor* searchAnchor(AnchorList* al, const char* str);
Anchor* searchURLLabel(struct _Buffer* buf, const char* url);
void reAnchorWord(struct _Buffer* buf, Line* l, int spos, int epos);
const char* reAnchor(struct _Buffer* buf, const char* re);
void addMultirowsForm(struct _Buffer* buf, AnchorList* al);
Anchor* closest_next_anchor(AnchorList* a, Anchor* an, int x, int y);
Anchor* closest_prev_anchor(AnchorList* a, Anchor* an, int x, int y);
void addMultirowsImg(struct _Buffer* buf, AnchorList* al);
HmarkerList* putHmarker(HmarkerList* ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList* a, HmarkerList* hl, int line,
    int pos, int shift);
char* getAnchorText(struct _Buffer* buf, AnchorList* al, Anchor* a);
struct _Buffer* link_list_panel(struct _Buffer* buf);
