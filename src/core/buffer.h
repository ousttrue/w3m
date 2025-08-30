#pragma once
#include "line.h"
#include "istream.h"
#include "anchor.h"
#include "linklist.h"

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

typedef struct _Buffer {
    char* filename;
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
    char* real_type;
    int allLine;
    enum BufferProperty bufferprop;
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
    char check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    struct form_item_list* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* header_source;
    char search_header;
    char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
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
void reshapeBuffer(struct _Buffer* buf);
void copyBuffer(struct _Buffer* a, struct _Buffer* b);
struct _Buffer* prevBuffer(struct _Buffer* first, struct _Buffer* buf);
int writeBufferCache(struct _Buffer* buf);
int readBufferCache(struct _Buffer* buf);
