#pragma once
#include "line.h"
#include "geometry.h"
#include "Document.h"
#include "ContentType.h"
#include "url.h"
#include "textlist.h"
#include <stdio.h>
#include <wc.h>

extern int nextpage_topline;
extern int REV_LB[];

#define SAVE_BUFPOSITION(sbufp) COPY_BUFPOSITION(sbufp, Currentbuf)
#define RESTORE_BUFPOSITION(sbufp) COPY_BUFPOSITION(Currentbuf, sbufp)

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

struct Buffer {
    const char* filename;
    const char* buffername;

    struct Document document;

    short width;
    int currentColumn;
    int pos;
    int visualpos;

    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    enum ContentType content_type;
    enum BufferProperty bufferprop;

    struct Url currentURL;

    int real_scheme;
    char* sourcefile;
    int* clone;
    size_t trbyte;
    bool check_url;
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
    struct Anchor* submit;
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
// void gotoRealLine(struct Buffer* buf, int n);
void gotoLine(struct Buffer* buf, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar);
void reshapeBuffer(struct UI ui, struct Buffer* buf, int cols);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
int readBufferCache(struct Buffer* buf);

bool applyCursor(struct Buffer* buf);
void arrangeCursor(struct Buffer* buf);
void arrangeLine(struct Buffer* buf);
void cursorXY(struct Buffer* buf, int x, int y);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
struct Url* baseURL(struct Buffer* buf);
int columnSkip(struct Buffer* buf, int offset);
struct LineList* lineSkip(struct Buffer* buf, struct LineList* line, int offset, int last);
struct LineList* currentLineSkip(struct Buffer* buf, struct LineList* line, int offset, int last);
char* last_modified(struct Buffer* buf);
struct Buffer* cookie_list_panel(struct UI ui);
struct Int2 viewportCursor(struct Buffer* buf);
struct Int2 updateCursor(struct Buffer* buf, struct Int2 viewport_size,
    struct Int2 viewport_cursor, struct Int2 cursor_delta, bool* hasScroll);
void reseq_anchor(struct Buffer* buf);

struct Anchor* retrieveCurrentAnchor(struct Buffer* buf);
struct Anchor* retrieveCurrentImg(struct Buffer* buf);
struct Anchor* retrieveCurrentForm(struct Buffer* buf);
struct Anchor* searchURLLabel(struct Buffer* buf, const char* url);
void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos);
const char* reAnchor(struct Buffer* buf, const char* re);
struct AnchorList;
void addMultirowsForm(struct Buffer* buf, struct AnchorList* al);
void addMultirowsImg(struct Buffer* buf, struct AnchorList* al);
const char* getAnchorText(struct Buffer* buf, struct AnchorList* al, struct Anchor* a);
struct BufferPoint getBufferPosition(struct Buffer* buf);

inline static void COPY_BUFPOSITION(struct Buffer* dstbuf, struct Buffer* srcbuf)
{
    (dstbuf)->document.topLineIndex = (srcbuf)->document.topLineIndex;
    (dstbuf)->document.currentLineIndex = (srcbuf)->document.currentLineIndex;
    (dstbuf)->pos = (srcbuf)->pos;
    (dstbuf)->visualpos = (srcbuf)->visualpos;
    (dstbuf)->currentColumn = (srcbuf)->currentColumn;
}
