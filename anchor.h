#pragma once

struct BufferPoint {
    int line;
    int pos;
    int invalid;
};

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
struct HtmlBuilder;
struct FormList;
struct parsed_tag;
struct Anchor* searchURLLabel(struct Buffer* buf, const char* url);
struct Anchor* searchAnchor(struct AnchorList* al, const char* str);
struct Anchor* registerHref(struct Buffer* buf,
    const char* url, const char* target,
    const char* referer, const char* title,
    unsigned char key, int line, int pos);
struct Anchor* registerImg(struct Buffer* buf,
    const char* url, const char* title,
    int line, int pos);
struct AnchorList* putAnchor(struct AnchorList* al,
    const char* url, const char* target,
    struct Anchor** anchor_return, const char* referer,
    const char* title, unsigned char key, int line,
    int pos);
struct Anchor* registerForm(struct HtmlBuilder* hb,
    struct Buffer* buf, struct FormList* flist,
    struct parsed_tag* tag, int line, int pos);
