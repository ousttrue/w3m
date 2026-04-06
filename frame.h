#pragma once

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
    struct _ParsedURL* baseURL;
    const char* source;
    char* type;
    char* referer;
    struct _anchorList* nameList;
    struct form_list* request;
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
    struct _ParsedURL* currentURL;
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
struct _Buffer;
struct frame_body* newFrame(struct HtmlTag* tag, struct _Buffer* buf);
struct frameset* newFrameSet(struct HtmlTag* tag);
void addFrameSetElement(struct frameset* f, union frameset_element element);
void deleteFrame(struct frame_body* b);
void deleteFrameSet(struct frameset* f);
void deleteFrameSetElement(union frameset_element e);
struct frameset* copyFrameSet(struct frameset* of);
void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs, struct _Buffer* buf);
struct frameset* popFrameTree(struct frameset_queue** fqpp);
struct form_list;
void resetFrameElement(union frameset_element* f_element, struct _Buffer* buf, char* referer, struct form_list* request);
struct _Buffer* renderFrame(struct _Buffer* Cbuf, int force_reload);
union frameset_element* search_frame(struct frameset* fset, char* name);
