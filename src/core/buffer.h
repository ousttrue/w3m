#pragma once
#include "line.h"
#include "ContentType.h"
#include "anchor.h"
#include "linklist.h"
#include "url.h"
#include "textlist.h"
#include <stdio.h>
#include <wc.h>

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#define DICTBUFFERNAME "*dictionary*"

extern int nextpage_topline;
extern int REV_LB[];

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

#define NO_BUFFER ((struct Buffer*)1)

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
    BP_CLOSE = 0x40,
};

typedef struct BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct BufferPos* next;
    struct BufferPos* prev;
} BufferPos;

struct Buffer {
    char* filename;
    const char* buffername;
    struct Line* firstLine;
    struct Line* topLine;
    struct Line* currentLine;
    struct Line* lastLine;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    short width;
    enum ContentType content_type;
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
    struct Form* formlist;
    struct _MapList* maplist;
    HmarkerList* hmarklist;
    HmarkerList* imarklist;
    struct Url currentURL;
    struct Url* baseURL;
    const char* baseTarget;
    int real_scheme;
    char* sourcefile;
    int* clone;
    size_t trbyte;
    bool check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    struct FormItem* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* ssl_certificate;
    char image_flag;
    char image_loaded;
    Anchor* submit;
    struct BufferPos* undo;
    struct _AlarmEvent* event;
};

#define _INIT_BUFFER_WIDTH (getCols() - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

struct Buffer* newBuffer();
struct Buffer* nullBuffer(void);
void clearBuffer(struct Buffer* buf);
void discardBuffer(struct Buffer* buf);
struct Buffer* namedBuffer(struct Buffer* first, char* name);
struct Buffer* deleteBuffer(struct Buffer* first, struct Buffer* delbuf);
struct Buffer* replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf);
struct Buffer* nthBuffer(struct Buffer* firstbuf, int n);
void gotoRealLine(struct Buffer* buf, int n);
void gotoLine(struct Buffer* buf, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar);
void reshapeBuffer(struct Buffer* buf, int cols);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
int readBufferCache(struct Buffer* buf);
void cursorUp0(struct Buffer* buf, int n);
void cursorUp(struct Buffer* buf, int n);
void cursorDown0(struct Buffer* buf, int n);
void cursorDown(struct Buffer* buf, int n);
void cursorUpDown(struct Buffer* buf, int n);
void cursorRight(struct Buffer* buf, int n);
void cursorLeft(struct Buffer* buf, int n);
void cursorHome(struct Buffer* buf);
void arrangeCursor(struct Buffer* buf);
void arrangeLine(struct Buffer* buf);
void cursorXY(struct Buffer* buf, int x, int y);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
char* url_decode2(const char* url, const struct Buffer* buf);
struct Url* baseURL(struct Buffer* buf);
int columnSkip(struct Buffer* buf, int offset);
struct Line* lineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
struct Line* currentLineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
char* last_modified(struct Buffer* buf);
struct Buffer* cookie_list_panel(void);
