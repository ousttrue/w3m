#pragma once
#include <stdio.h>
#include "tab.h"
#include "line.h"
#include "url.h"

#define _INIT_BUFFER_WIDTH (COLS - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

/* Link struct Buffer */
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

/* struct Buffer Property */
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

struct Buffer {
    const char* filename;
    char* buffername;
    struct Line* firstLine;
    struct Line* topLine;
    struct Line* currentLine;
    struct Line* lastLine;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
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
    struct Form* formlist;
    struct _MapList* maplist;
    struct HmarkerList* hmarklist;
    struct HmarkerList* imarklist;
    struct Url currentURL;
    struct Url* baseURL;
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
    struct FormItem* form_submit;
    char* savecache;
    const char* edit;
    struct mailcap* mailcap;
    const char* mailcap_source;
    char* header_source;
    char search_header;
    const char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    struct Anchor* submit;
    struct _BufferPos* undo;
    struct _AlarmEvent* event;
};

#define NO_BUFFER ((struct Buffer*)1)

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
 * global struct Buffer *Currentbuf;
 * global struct Buffer *Firstbuf;
 */
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)
#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

extern struct Buffer* newBuffer(int width);
extern struct Buffer* nullBuffer(void);
extern void clearBuffer(struct Buffer* buf);
extern void discardBuffer(struct Buffer* buf);
extern struct Buffer* namedBuffer(struct Buffer* first, char* name);
extern struct Buffer* deleteBuffer(struct Buffer* first, struct Buffer* delbuf);
extern struct Buffer* replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf);
extern struct Buffer* nthBuffer(struct Buffer* firstbuf, int n);
extern void gotoRealLine(struct Buffer* buf, int n);
extern void gotoLine(struct Buffer* buf, int n);
extern struct Buffer* selectBuffer(struct CmdArgs* args, struct Buffer* firstbuf, struct Buffer* currentbuf,
    char* selectchar);
extern void reshapeBuffer(struct CmdArgs* args, struct Buffer* buf);
extern void copyBuffer(struct Buffer* a, struct Buffer* b);
extern struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
extern int writeBufferCache(struct Buffer* buf);
extern int readBufferCache(struct Buffer* buf);
struct URLFile;
extern struct Buffer* loadBuffer(struct CmdArgs* args, struct URLFile* uf, struct Buffer* newBuf);
extern struct Buffer* loadImageBuffer(struct CmdArgs* args, struct URLFile* uf, struct Buffer* newBuf);
extern void saveBuffer(struct Buffer* buf, FILE* f, int cont);
extern void saveBufferBody(struct Buffer* buf, FILE* f, int cont);
extern struct Buffer* getshell(struct CmdArgs* args, const char* cmd);
extern struct Buffer* getpipe(const char* cmd);
typedef union input_stream* InputStream;
extern struct Buffer* openPagerBuffer(InputStream stream, struct Buffer* buf);
extern struct Buffer* openGeneralPagerBuffer(struct CmdArgs* args, InputStream stream);
extern struct Line* getNextPage(struct Buffer* buf, int plen);
extern struct Buffer* doExternal(struct CmdArgs* args, struct URLFile uf, const char* type, struct Buffer* defaultbuf);
extern void cursorUp0(struct Buffer* buf, int n);
extern void cursorUp(struct Buffer* buf, int n);
extern void cursorDown0(struct Buffer* buf, int n);
extern void cursorDown(struct Buffer* buf, int n);
extern void cursorUpDown(struct Buffer* buf, int n);
extern void cursorRight(struct Buffer* buf, int n);
extern void cursorLeft(struct Buffer* buf, int n);
extern void cursorHome(struct Buffer* buf);
extern void arrangeCursor(struct Buffer* buf);
extern void arrangeLine(struct Buffer* buf);
extern void cursorXY(struct Buffer* buf, int x, int y);
extern void restorePosition(struct Buffer* buf, struct Buffer* orig);
extern int columnSkip(struct Buffer* buf, int offset);
extern void reAnchorWord(struct Buffer* buf, struct Line* l, int spos, int epos);
extern const char* reAnchor(struct Buffer* buf, const char* re);
extern const char* reAnchorNews(struct Buffer* buf, const char* re);
extern char* reAnchorNewsheader(struct Buffer* buf);
extern struct Buffer* link_list_panel(struct Buffer* buf);
extern void chkURLBuffer(struct Buffer* buf);
extern void chkNMIDBuffer(struct Buffer* buf);
extern int currentLn(struct Buffer* buf);
extern void tmpClearBuffer(struct Buffer* buf);
