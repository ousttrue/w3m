#pragma once
#include <w3m.h>

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
    const char* name;
    const char* url;
    struct Url* baseURL;
    const char* source;
    const char* type;
    const char* referer;
    struct AnchorList* nameList;
    struct Form* request;
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
    const char* name;
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

struct HtmlTag;
struct Buffer;
struct frame_body* newFrame(struct HtmlTag* tag, struct Buffer* buf);
struct frameset* newFrameSet(struct HtmlTag* tag);
void addFrameSetElement(struct frameset* f, union frameset_element element);
void deleteFrame(struct frame_body* b);
void deleteFrameSet(struct frameset* f);
void deleteFrameSetElement(union frameset_element e);
struct frameset* copyFrameSet(struct frameset* of);
void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs, struct Buffer* buf);
struct frameset* popFrameTree(struct frameset_queue** fqpp);
struct Form;
void resetFrameElement(union frameset_element* f_element, struct Buffer* buf, const char* referer, struct Form* request);
struct Buffer* renderFrame(struct CmdArgs args, struct Buffer* Cbuf, int force_reload);
union frameset_element* search_frame(struct frameset* fset, const char* name);
