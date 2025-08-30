#pragma once

typedef struct {
    int line;
    int pos;
    int invalid;
} BufferPoint;

typedef struct _anchor {
    char* url;
    char* target;
    char* referer;
    char* title;
    unsigned char accesskey;
    BufferPoint start;
    BufferPoint end;
    int hseq;
    char slave;
    short y;
    short rows;
    struct _image* image;
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
struct form_list;
struct parsed_tag;

AnchorList* putAnchor(AnchorList* al, char* url, char* target,
    Anchor** anchor_return, char* referer,
    char* title, unsigned char key, int line,
    int pos);
Anchor* registerHref(struct _Buffer* buf, char* url, char* target,
    char* referer, char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(struct _Buffer* buf, char* url, int line, int pos);
Anchor* registerImg(struct _Buffer* buf, char* url, char* title, int line,
    int pos);
Anchor* registerForm(struct _Buffer* buf, struct form_list* flist,
    struct parsed_tag* tag, int line, int pos);
int onAnchor(Anchor* a, int line, int pos);
Anchor* retrieveAnchor(AnchorList* al, int line, int pos);
Anchor* retrieveCurrentAnchor(struct _Buffer* buf);
Anchor* retrieveCurrentImg(struct _Buffer* buf);
Anchor* retrieveCurrentForm(struct _Buffer* buf);
Anchor* searchAnchor(AnchorList* al, char* str);
Anchor* searchURLLabel(struct _Buffer* buf, char* url);
void reAnchorWord(struct _Buffer* buf, Line* l, int spos, int epos);
char* reAnchor(struct _Buffer* buf, char* re);
void addMultirowsForm(struct _Buffer* buf, AnchorList* al);
Anchor* closest_next_anchor(AnchorList* a, Anchor* an, int x, int y);
Anchor* closest_prev_anchor(AnchorList* a, Anchor* an, int x, int y);
void addMultirowsImg(struct _Buffer* buf, AnchorList* al);
HmarkerList* putHmarker(HmarkerList* ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList* a, HmarkerList* hl, int line,
    int pos, int shift);
char* getAnchorText(struct _Buffer* buf, AnchorList* al, Anchor* a);
struct _Buffer* link_list_panel(struct _Buffer* buf);
