#pragma once
#include "image.h"

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct Anchor {
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
    Image* image;

    const char* url;
    struct form_item_list* formitem;
} Anchor;

typedef struct AnchorList {
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

AnchorList* putAnchor(AnchorList* al, const char* url, struct form_item_list* formitem,
    const char* target,
    Anchor** anchor_return, const char* referer,
    const char* title, unsigned char key, int line,
    int pos);
struct _Buffer;
Anchor* registerHref(struct _Buffer* buf, const char* url, const char* target,
    const char* referer, const char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(struct _Buffer* buf, const char* url, int line, int pos);
Anchor* registerImg(struct _Buffer* buf, const char* url, const char* title, int line,
    int pos);
struct parsed_tag;
struct form_list;
Anchor* registerForm(struct _Buffer* buf, struct form_list* flist,
    struct parsed_tag* tag, int line, int pos);
int onAnchor(Anchor* a, int line, int pos);
Anchor* retrieveAnchor(AnchorList* al, int line, int pos);
Anchor* retrieveCurrentAnchor(struct _Buffer* buf);
Anchor* retrieveCurrentImg(struct _Buffer* buf);
Anchor* retrieveCurrentForm(struct _Buffer* buf);
Anchor* searchAnchor(AnchorList* al, const char* str);
Anchor* searchURLLabel(struct _Buffer* buf, const char* url);
struct _Line;
void reAnchorWord(struct _Buffer* buf, struct _Line* l, int spos, int epos);
const char* reAnchor(struct _Buffer* buf, const char* re);
const char* reAnchorNews(struct _Buffer* buf, const char* re);
char* reAnchorNewsheader(struct _Buffer* buf);
