/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_FRAME_H
#define W3M_FRAME_H

#include "anchor.h"
#include "buffer.h"
#include "config.h"
#include "form.h"
#include "html.h"
#include "parsetagx.h"

struct frame_element {
    char attr;
#define F_UNLOADED 0x00
#define F_BODY 0x01
#define F_FRAMESET 0x02
    char dummy;
    char* name;
};

#define FB_NO_BUFFER 0x01
struct frame_body {
    char attr;
    char flags;
    const char* name;
    const char* url;
    ParsedURL* baseURL;
    const char* source;
    const char* type;
    const char* referer;
    AnchorList* nameList;
    FormList* request;
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
    ParsedURL* currentURL;
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
    AnchorList* formitem;
};

extern struct frame_body* newFrame(struct parsed_tag* tag, Buffer* buf);
extern struct frameset* newFrameSet(struct parsed_tag* tag);
extern void addFrameSetElement(struct frameset* f,
    union frameset_element element);
extern void deleteFrame(struct frame_body* b);
extern void deleteFrameSet(struct frameset* f);
extern void deleteFrameSetElement(union frameset_element e);
extern struct frameset* copyFrameSet(struct frameset* of);
extern void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs,
    Buffer* buf);
extern struct frameset* popFrameTree(struct frameset_queue** fqpp);
extern void resetFrameElement(union frameset_element* f_element, Buffer* buf,
    const char* referer, FormList* request);
extern Buffer* renderFrame(Buffer* Cbuf, int force_reload);
extern union frameset_element* search_frame(struct frameset* fset, const char* name);

extern struct frameset* renderFrameSet;

#endif
