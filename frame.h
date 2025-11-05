/*
 * frame support
 */
#include "Url.h"

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
    char* source;
    char* type;
    char* referer;
    struct _AnchorList* nameList;
    struct from_list* request;
    char* ssl_certificate;
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
    struct _AnchorList* formitem;
};

extern struct frameset* renderFrameSet;

struct parsed_tag;
struct frameset* newFrameSet(struct parsed_tag* tag);
void addFrameSetElement(struct frameset* f,
    union frameset_element element);
void deleteFrame(struct frame_body* b);
void deleteFrameSet(struct frameset* f);
void deleteFrameSetElement(union frameset_element e);
struct frameset* copyFrameSet(struct frameset* of);
struct frameset* popFrameTree(struct frameset_queue** fqpp);
union frameset_element* search_frame(struct frameset* fset, char* name);
