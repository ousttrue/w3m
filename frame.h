#pragma once
#include "anchor_list.h"
/*
 * frame support
 */

struct frame_element {
    char attr;
#define F_UNLOADED 0x00
#define F_BODY 0x01
#define F_FRAMESET 0x02
    char dummy;
    char* name;
};

struct frame_body {
    char attr;
    char flags;
#define FB_NO_BUFFER 0x01
    char* name;
    char* url;
    struct Url* baseURL;
    const char* source;
    const char* type;
    const char* referer;
    struct AnchorList nameList;
    struct FormList* request;
    const char* ssl_certificate;
};

union frameset_element {
    struct frame_element* element;
    struct frame_body* body;
    struct frameset* set;
};

struct frameset {
    char attr;
    char dummy;
    char* name;
    struct Url* currentURL;
    char** width;
    char** height;
    int col;
    int row;
    int i;
    union frameset_element* frame;
};

struct frameset_queue {
    struct frameset_queue* next;
    struct frameset_queue* back;
    struct frameset* frameset;
    long linenumber;
    long top_linenumber;
    int pos;
    int currentColumn;
    struct AnchorList* formitem;
};

extern struct frameset* renderFrameSet;
extern union frameset_element* search_frame(struct frameset* fset, const char* name);
struct Buffer;
extern void resetFrameElement(union frameset_element* f_element, struct Buffer* buf,
    const char* referer, struct FormList* request);

struct HtmlTag;
extern struct frame_body* newFrame(struct HtmlTag* tag, struct Buffer* buf);
extern struct frameset* newFrameSet(struct HtmlTag* tag);
extern void addFrameSetElement(struct frameset* f,
    union frameset_element element);
extern void deleteFrame(struct frame_body* b);
extern void deleteFrameSet(struct frameset* f);
extern void deleteFrameSetElement(union frameset_element e);
extern struct frameset* copyFrameSet(struct frameset* of);
extern void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs,
    struct Buffer* buf);
extern struct frameset* popFrameTree(struct frameset_queue** fqpp);
extern struct Buffer* renderFrame(struct Buffer* Cbuf, int force_reload);
