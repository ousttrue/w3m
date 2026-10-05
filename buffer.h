#pragma once
#include "line.h"
#include "anchor.h"
#include "form.h"
#include "map.h"
#include "url.h"

/* Flags for displayBuffer() */
#define B_NORMAL 0
#define B_FORCE_REDRAW 1
#define B_REDRAW 2
#define B_SCROLL 3
#define B_REDRAW_IMAGE 4

/* Buffer Property */
#define BP_NORMAL 0x0
#define BP_PIPE 0x1
#define BP_FRAME 0x2
#define BP_INTERNAL 0x8
#define BP_NO_URL 0x10
#define BP_REDIRECTED 0x20
#define BP_CLOSE 0x40

/* Link Buffer */
#define LB_NOLINK -1
#define LB_FRAME 0 /* rFrame() */
#define LB_N_FRAME 1
#define LB_INFO 2 /* pginfo() */
#define LB_N_INFO 3
#define LB_SOURCE 4 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 5

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

#define LINK_TYPE_NONE 0
#define LINK_TYPE_REL 1
#define LINK_TYPE_REV 2
typedef struct _LinkList {
    char* url;
    char* title; /* Next, Contents, ... */
    char* ctype; /* Content-Type */
    char type; /* Rel, Rev */
    struct _LinkList* next;
} LinkList;

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
    const char* filename;
    const char* buffername;
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
    short cols;
    short lines;
    struct input_stream* pagerSource;
    AnchorList* hrefList;
    AnchorList* nameList;
    AnchorList* imgList;
    AnchorList* formList;
    LinkList* linklist;
    FormList* formlist;
    struct MapList* maplist;
    HmarkerList* hmarklist;
    HmarkerList* imarklist;
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
    uint8_t auto_detect;
    struct TextList* document_header;
    FormItemList* form_submit;
    const char* savecache;
    const char* edit;
    struct mailcap* mailcap;
    const char* mailcap_source;
    const char* header_source;
    char search_header;
    const char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor* submit;
    struct _BufferPos* undo;
    struct AlarmEvent* event;
    int mainline;
} Buffer;

#define TOP_LINENUMBER(buf) ((buf)->topLine ? (buf)->topLine->linenumber : 1)
#define CUR_LINENUMBER(buf) ((buf)->currentLine ? (buf)->currentLine->linenumber : 1)

Buffer* deleteBuffer(Buffer* first, Buffer* delbuf);
Buffer* newBuffer(int width);
Buffer* nthBuffer(Buffer* firstbuf, int n);
Buffer* nullBuffer(void);
Buffer* prevBuffer(Buffer* first, const Buffer* buf);
Buffer* replaceBuffer(Buffer* first, Buffer* delbuf, Buffer* newbuf);
Buffer* selectBuffer(Buffer* firstbuf, Buffer* currentbuf, char* selectchar);
char* getCurWord(Buffer* buf, int* spos, int* epos);
struct Str GetWord(Buffer* buf);
int readBufferCache(Buffer* buf);
int writeBufferCache(Buffer* buf);
void chkURLBuffer(Buffer* buf);
void chkNMIDBuffer(Buffer* buf);
void clearBuffer(Buffer* buf);
void copyBuffer(Buffer* a, Buffer* b);
void discardBuffer(Buffer* buf);
void gotoLine(Buffer* buf, int n);
void gotoRealLine(Buffer* buf, int n);
void reshapeBuffer(Buffer* buf);
void tmpClearBuffer(Buffer* buf);

int columnSkip(Buffer* buf, int offset);

Line* lineSkip(Buffer* buf, Line* line, int offset, int last);
Line* currentLineSkip(Buffer* buf, Line* line, int offset, int last);
pStr guess_save_name(Buffer* buf, const char* file);
void cmd_loadBuffer(Buffer* buf, int prop, int linkid);
