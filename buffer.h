#pragma once
#include <stdio.h>
#include "tab.h"
#include "line.h"
#include "url.h"

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

/* Link Buffer */
#define LB_NOLINK -1
#define LB_FRAME 0 /* rFrame() */
#define LB_N_FRAME 1
#define LB_INFO 2 /* pginfo() */
#define LB_N_INFO 3
#define LB_SOURCE 4 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 5

extern int REV_LB[];

typedef struct _BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct _BufferPos* next;
    struct _BufferPos* prev;
} BufferPos;

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

typedef struct _Buffer {
    const char* filename;
    char* buffername;
    Line* firstLine;
    Line* topLine;
    Line* currentLine;
    Line* lastLine;
    struct _Buffer* nextBuffer;
    struct _Buffer* linkBuffer[MAX_LB];
    short width;
    short height;
    char* type;
    const char* real_type;
    int allLine;
    short bufferprop;
    int currentColumn;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    InputStream pagerSource;
    struct AnchorList* href;
    struct AnchorList* name;
    struct AnchorList* img;
    struct AnchorList* formitem;
    struct LinkList* linklist;
    struct form_list* formlist;
    struct _MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;
    struct _ParsedURL currentURL;
    struct _ParsedURL* baseURL;
    char* baseTarget;
    int real_scheme;
    const char* sourcefile;
    struct frameset* frameset;
    struct frameset_queue* frameQ;
    int* clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    struct _textlist* document_header;
    struct form_item_list* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* header_source;
    char search_header;
    const char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    struct Anchor* submit;
    struct _BufferPos* undo;
    struct _AlarmEvent* event;
} Buffer;

#define NO_BUFFER ((Buffer*)1)

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->COLS = (srcbuf)->COLS;   \
        (dstbuf)->LINES = (srcbuf)->LINES; \
    }

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

/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

extern Buffer* newBuffer(int width);
extern Buffer* nullBuffer(void);
extern void clearBuffer(Buffer* buf);
extern void discardBuffer(Buffer* buf);
extern Buffer* namedBuffer(Buffer* first, char* name);
extern Buffer* deleteBuffer(Buffer* first, Buffer* delbuf);
extern Buffer* replaceBuffer(Buffer* first, Buffer* delbuf, Buffer* newbuf);
extern Buffer* nthBuffer(Buffer* firstbuf, int n);
extern void gotoRealLine(Buffer* buf, int n);
extern void gotoLine(Buffer* buf, int n);
extern Buffer* selectBuffer(Buffer* firstbuf, Buffer* currentbuf,
    char* selectchar);
extern void reshapeBuffer(Buffer* buf);
extern void copyBuffer(Buffer* a, Buffer* b);
extern Buffer* prevBuffer(Buffer* first, Buffer* buf);
extern int writeBufferCache(Buffer* buf);
extern int readBufferCache(Buffer* buf);
struct URLFile;
extern Buffer* loadBuffer(struct URLFile* uf, Buffer* newBuf);
extern Buffer* loadImageBuffer(struct URLFile* uf, Buffer* newBuf);
extern void saveBuffer(Buffer* buf, FILE* f, int cont);
extern void saveBufferBody(Buffer* buf, FILE* f, int cont);
extern Buffer* getshell(const char* cmd);
extern Buffer* getpipe(const char* cmd);
typedef union input_stream* InputStream;
extern Buffer* openPagerBuffer(InputStream stream, Buffer* buf);
extern Buffer* openGeneralPagerBuffer(InputStream stream);
extern struct _Line* getNextPage(Buffer* buf, int plen);
extern Buffer* doExternal(struct URLFile uf, const char* type, Buffer* defaultbuf);
extern void cursorUp0(Buffer* buf, int n);
extern void cursorUp(Buffer* buf, int n);
extern void cursorDown0(Buffer* buf, int n);
extern void cursorDown(Buffer* buf, int n);
extern void cursorUpDown(Buffer* buf, int n);
extern void cursorRight(Buffer* buf, int n);
extern void cursorLeft(Buffer* buf, int n);
extern void cursorHome(Buffer* buf);
extern void arrangeCursor(Buffer* buf);
extern void arrangeLine(Buffer* buf);
extern void cursorXY(Buffer* buf, int x, int y);
extern void restorePosition(Buffer* buf, Buffer* orig);
extern int columnSkip(Buffer* buf, int offset);
extern void reAnchorWord(Buffer* buf, Line* l, int spos, int epos);
extern char* reAnchor(Buffer* buf, char* re);
extern char* reAnchorNews(Buffer* buf, char* re);
extern char* reAnchorNewsheader(Buffer* buf);
extern Buffer* link_list_panel(Buffer* buf);
extern void chkURLBuffer(Buffer* buf);
extern void chkNMIDBuffer(Buffer* buf);
extern int currentLn(Buffer* buf);
extern void tmpClearBuffer(Buffer* buf);
