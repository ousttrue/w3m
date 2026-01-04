#pragma once
#include "url.h"
#include "textlist.h"
#include <stddef.h>
#include <libwc/wc_types.h>

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
struct LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct LinkList* next;
};

/* Link Buffer */
enum LinkBufferID {
    LB_NOLINK = -1,
    LB_FRAME = 0, /* rFrame() */
    LB_N_FRAME = 1,
    LB_INFO = 2, /* pginfo() */
    LB_N_INFO = 3,
    LB_SOURCE = 4, /* vwSrc() */
    LB_N_SOURCE = LB_SOURCE,
    MAX_LB = 5,
};

struct BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct BufferPos* next;
    struct BufferPos* prev;
};

/* Buffer Property */
#define BP_NORMAL 0x0
#define BP_PIPE 0x1
#define BP_FRAME 0x2
#define BP_INTERNAL 0x8
#define BP_NO_URL 0x10
#define BP_REDIRECTED 0x20
#define BP_CLOSE 0x40

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

#include "content.h"
#include "document.h"

struct Buffer {
    struct Content content;
    struct Document doc;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    short bufferprop;

    int* clone;
    char check_url;
    char* savecache;
    char* edit;
    struct _AlarmEvent* event;
};

struct Url* baseURL(struct Buffer* buf);
char* url_decode2(const char* url, const struct Buffer* buf);
void delBuffer(struct Buffer* buf);
void cmd_loadBuffer(struct Buffer* buf, int prop, enum LinkBufferID linkid);
bool readBufferCache(struct Buffer* buf);
int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y);
extern void reshapeBuffer(struct Buffer* buf);
void reAnchorWord(struct Buffer* buf, struct Line* l, int spos, int epos);
extern void saveBuffer(struct Buffer* buf, FILE* f, int cont);
extern void saveBufferBody(struct Buffer* buf, FILE* f, int cont);
extern struct Buffer* getshell(char* cmd);
extern struct Buffer* newBuffer(int width);
extern struct Buffer* nullBuffer(void);
extern void clearBuffer(struct Buffer* buf);
extern void discardBuffer(struct Buffer* buf);
extern struct Buffer* namedBuffer(struct Buffer* first, char* name);
extern struct Buffer* deleteBuffer(struct Buffer* first, struct Buffer* delbuf);
extern struct Buffer* replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf);
extern struct Buffer* nthBuffer(struct Buffer* firstbuf, int n);
extern struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf,
    char* selectchar);
extern void copyBuffer(struct Buffer* a, struct Buffer* b);
extern struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
extern int writeBufferCache(struct Buffer* buf);
