#pragma once
#include "line.h"
#include "anchor.h"
#include "linklist.h"
#include "url.h"

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#define DICTBUFFERNAME "*dictionary*"

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->topLine = (srcbuf)->topLine;             \
        (dstbuf)->currentLine = (srcbuf)->currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }

#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

#define NO_BUFFER ((Buffer*)1)

enum LinkBufferType {
    LB_NOLINK = -1,
    LB_INFO = 0 /* pginfo() */,
    LB_N_INFO = 1,
    LB_SOURCE = 2 /* vwSrc() */,
    LB_N_SOURCE = LB_SOURCE,
    MAX_LB = 3,
};

enum BufferProperty {
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};

typedef struct _BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct _BufferPos* next;
    struct _BufferPos* prev;
} BufferPos;

typedef struct _Buffer {
    char* filename;
    const char* buffername;
    Line* firstLine;
    Line* topLine;
    Line* currentLine;
    Line* lastLine;
    struct _Buffer* nextBuffer;
    struct _Buffer* linkBuffer[MAX_LB];
    short width;
    char* type;
    const char* real_type;
    int allLine;
    enum BufferProperty bufferprop;
    int currentColumn;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
    AnchorList* href;
    AnchorList* name;
    AnchorList* img;
    AnchorList* formitem;
    LinkList* linklist;
    struct form_list* formlist;
    struct _MapList* maplist;
    HmarkerList* hmarklist;
    HmarkerList* imarklist;
    ParsedURL currentURL;
    ParsedURL* baseURL;
    char* baseTarget;
    int real_scheme;
    char* sourcefile;
    int* clone;
    size_t trbyte;
    bool check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    struct form_item_list* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* ssl_certificate;
    char image_flag;
    char image_loaded;
    Anchor* submit;
    struct _BufferPos* undo;
    struct _AlarmEvent* event;
} Buffer;

#define _INIT_BUFFER_WIDTH (getCols() - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

struct _Buffer* newBuffer();
struct _Buffer* nullBuffer(void);
void clearBuffer(struct _Buffer* buf);
void discardBuffer(struct _Buffer* buf);
struct _Buffer* namedBuffer(struct _Buffer* first, char* name);
struct _Buffer* deleteBuffer(struct _Buffer* first, struct _Buffer* delbuf);
struct _Buffer* replaceBuffer(struct _Buffer* first, struct _Buffer* delbuf, struct _Buffer* newbuf);
struct _Buffer* nthBuffer(struct _Buffer* firstbuf, int n);
void gotoRealLine(struct _Buffer* buf, int n);
void gotoLine(struct _Buffer* buf, int n);
struct _Buffer* selectBuffer(struct _Buffer* firstbuf, struct _Buffer* currentbuf, char* selectchar);
void reshapeBuffer(struct _Buffer* buf, int cols);
void copyBuffer(struct _Buffer* a, struct _Buffer* b);
struct _Buffer* prevBuffer(struct _Buffer* first, struct _Buffer* buf);
int writeBufferCache(struct _Buffer* buf);
int readBufferCache(struct _Buffer* buf);
void cursorUp0(struct _Buffer* buf, int n);
void cursorUp(struct _Buffer* buf, int n);
void cursorDown0(struct _Buffer* buf, int n);
void cursorDown(struct _Buffer* buf, int n);
void cursorUpDown(struct _Buffer* buf, int n);
void cursorRight(struct _Buffer* buf, int n);
void cursorLeft(struct _Buffer* buf, int n);
void cursorHome(struct _Buffer* buf);
void arrangeCursor(struct _Buffer* buf);
void arrangeLine(struct _Buffer* buf);
void cursorXY(struct _Buffer* buf, int x, int y);
void restorePosition(struct _Buffer* buf, struct _Buffer* orig);
void saveBuffer(struct _Buffer* buf, FILE* f, int cont);
