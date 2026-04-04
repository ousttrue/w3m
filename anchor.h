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

struct HmarkerList {
    BufferPoint* marks;
    int nmark;
    int markmax;
    int prevhseq;
};

AnchorList* putAnchor(AnchorList* al, char* url, char* target,
    Anchor** anchor_return, char* referer,
    char* title, unsigned char key, int line,
    int pos);
typedef struct _Buffer Buffer;
Anchor* registerHref(Buffer* buf, char* url, char* target,
    char* referer, char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(Buffer* buf, char* url, int line, int pos);
Anchor* registerImg(Buffer* buf, char* url, char* title, int line,
    int pos);
struct parsed_tag;
struct form_list;
Anchor* registerForm(Buffer* buf, struct form_list* flist,
    struct parsed_tag* tag, int line, int pos);
int onAnchor(Anchor* a, int line, int pos);
Anchor* retrieveAnchor(AnchorList* al, int line, int pos);
Anchor* retrieveCurrentAnchor(Buffer* buf);
Anchor* retrieveCurrentImg(Buffer* buf);
Anchor* retrieveCurrentForm(Buffer* buf);
Anchor* searchAnchor(AnchorList* al, const char* str);
Anchor* searchURLLabel(Buffer* buf, const char* url);
Anchor* accesskey_menu(Buffer* buf);
Anchor* accesskey_menu(Buffer* buf);
struct form_item_list;
void formRecheckRadio(Anchor* a, Buffer* buf, struct form_item_list* form);
void formUpdateBuffer(Anchor* a, Buffer* buf, struct form_item_list* form);
void formResetBuffer(Buffer* buf, AnchorList* formitem);
void addMultirowsForm(Buffer* buf, AnchorList* al);
Anchor* closest_next_anchor(AnchorList* a, Anchor* an, int x, int y);
Anchor* closest_prev_anchor(AnchorList* a, Anchor* an, int x, int y);
void addMultirowsImg(Buffer* buf, AnchorList* al);
struct HmarkerList* putHmarker(struct HmarkerList* ml, int line, int pos, int seq);
void shiftAnchorPosition(AnchorList* a, struct HmarkerList* hl, int line, int pos, int shift);
char* getAnchorText(Buffer* buf, AnchorList* al, Anchor* a);
Anchor* list_menu(Buffer* buf);
