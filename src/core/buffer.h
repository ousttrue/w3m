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

#define COPY_BUFPOSITION(dstbuf, srcbuf)                         \
    {                                                            \
        (dstbuf)->topLineIndex = (srcbuf)->topLineIndex;         \
        (dstbuf)->currentLineIndex = (srcbuf)->currentLineIndex; \
        (dstbuf)->pos = (srcbuf)->pos;                           \
        (dstbuf)->visualpos = (srcbuf)->visualpos;               \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn;       \
    }

#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)

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
    struct BufferPos* next;
    struct BufferPos* prev;
} BufferPos;

struct Buffer {
    char* filename;
    const char* buffername;

    struct LineList* firstLine;
    int topLineIndex;
    int currentLineIndex;
    int allLine;

    short width;
    int currentColumn;
    int pos;
    int visualpos;

    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    enum ContentType content_type;
    enum BufferProperty bufferprop;
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
struct LineList* currentLine(struct Buffer* buf);
struct LineList* lastLine(struct Buffer* buf);
struct LineList* topLine(struct Buffer* buf);
// void gotoRealLine(struct Buffer* buf, int n);
void gotoLine(struct Buffer* buf, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar);
void reshapeBuffer(struct Buffer* buf, int cols);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
int readBufferCache(struct Buffer* buf);

void arrangeCursor(struct Buffer* buf);
void arrangeLine(struct Buffer* buf);
void cursorXY(struct Buffer* buf, int x, int y);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
char* url_decode2(const char* url, const struct Buffer* buf);
struct Url* baseURL(struct Buffer* buf);
int columnSkip(struct Buffer* buf, int offset);
struct LineList* lineSkip(struct Buffer* buf, struct LineList* line, int offset, int last);
struct LineList* currentLineSkip(struct Buffer* buf, struct LineList* line, int offset, int last);
char* last_modified(struct Buffer* buf);
struct Buffer* cookie_list_panel(void);
struct Int2 updateCursor(struct Buffer* buf, struct Int2 viewport_size,
    struct Int2 viewport_cursor, struct Int2 cursor_delta, bool *hasScroll);
