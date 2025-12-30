#pragma once
#include "url.h"
#include "textlist.h"
#include <stddef.h>
#include <libwc/wc_types.h>

#define NO_BUFFER ((struct Buffer*)1)

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
    const char* buffername;
    struct Document doc;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    short width;
    char* type;
    short bufferprop;
    struct AnchorList* name;
    struct AnchorList* formitem;
    struct LinkList* linklist;
    struct FormList* formlist;
    struct MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;
    struct Url currentURL;
    struct Url* baseURL;
    char* baseTarget;
    const char* sourcefile;
    struct frameset* frameset;
    struct frameset_queue* frameQ;
    int* clone;
    size_t trbyte;
    char check_url;
    enum wc_ces document_charset;
    wc_uint8 auto_detect;
    struct FormItemList* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    const char* mailcap_source;
    const char* header_source;
    const char* ssl_certificate;
    // char image_loaded;
    struct Anchor* submit;
    struct BufferPos* undo;
    struct _AlarmEvent* event;
};

#define COPY_BUFROOT(dstbuf, srcbuf)               \
    {                                              \
        (dstbuf)->doc.rootX = (srcbuf)->doc.rootX; \
        (dstbuf)->doc.rootY = (srcbuf)->doc.rootY; \
        (dstbuf)->doc.COLS = (srcbuf)->doc.COLS;   \
        (dstbuf)->doc.LINES = (srcbuf)->doc.LINES; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                           \
    {                                                              \
        (dstbuf)->doc.topLine = (srcbuf)->doc.topLine;             \
        (dstbuf)->doc.currentLine = (srcbuf)->doc.currentLine;     \
        (dstbuf)->doc.pos = (srcbuf)->doc.pos;                             \
        (dstbuf)->doc.cursorX = (srcbuf)->doc.cursorX;             \
        (dstbuf)->doc.cursorY = (srcbuf)->doc.cursorY;             \
        (dstbuf)->doc.visualpos = (srcbuf)->doc.visualpos;         \
        (dstbuf)->doc.currentColumn = (srcbuf)->doc.currentColumn; \
    }
#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->doc.topLine ? (buf)->doc.topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->doc.currentLine ? (buf)->doc.currentLine->linenumber : 1)

struct Url* baseURL(struct Buffer* buf);
char* url_decode2(const char* url, const struct Buffer* buf);
void delBuffer(struct Buffer* buf);
void cmd_loadBuffer(struct Buffer* buf, int prop, enum LinkBufferID linkid);
bool readBufferCache(struct Buffer* buf);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
void cursorXY(struct Buffer* buf, int x, int y);
void cursorUp(struct Buffer* buf, int n);
void cursorDown(struct Buffer* buf, int n);
void cursorRight(struct Buffer* buf, int n);
void cursorLeft(struct Buffer* buf, int n);
void cursorHome(struct Buffer* buf);
void arrangeCursor(struct Buffer* buf);
int columnSkip(struct Buffer* buf, int offset);
int getMapXY(struct Buffer* buf, struct Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(struct Buffer* buf);
extern struct Anchor* retrieveCurrentAnchor(struct Buffer* buf);
extern struct Anchor* retrieveCurrentImg(struct Buffer* buf);
extern struct Anchor* retrieveCurrentForm(struct Buffer* buf);
extern struct Anchor* retrieveCurrentMap(struct Buffer* buf);
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
extern void gotoRealLine(struct Buffer* buf, int n);
extern struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf,
    char* selectchar);
extern void copyBuffer(struct Buffer* a, struct Buffer* b);
extern struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
extern int writeBufferCache(struct Buffer* buf);
