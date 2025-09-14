#pragma once
#include "line.h"
#include "geometry.h"
#include "Content.h"
#include "Document.h"
#include "url.h"
#include "textlist.h"
#include <stdio.h>
#include <wc.h>

extern int nextpage_topline;

struct Buffer {
    const char* filename;

    char* ssl_certificate;
    TextList* document_header;

    struct Content content;

    struct Document document;

    struct Buffer* nextBuffer;

    int* clone;
    bool check_url;
    wc_uint8 auto_detect;
    struct FormItem* form_submit;
    char* savecache;
    char* edit;
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
void gotoLine(struct Document* doc, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar);
void reshapeBuffer(struct UI ui, struct Buffer* buf, int cols);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
int readBufferCache(struct Buffer* buf);

void cursorXY(struct Buffer* buf, int x, int y);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
struct Url* baseURL(struct Buffer* buf);
int columnSkip(struct Buffer* buf, int offset);
char* last_modified(struct Buffer* buf);
struct Content cookie_list_panel(struct UI ui);
void reseq_anchor(struct Buffer* buf);

struct Anchor* retrieveCurrentAnchor(struct Buffer* buf);
struct Anchor* retrieveCurrentImg(struct Buffer* buf);
struct Anchor* retrieveCurrentForm(struct Document* doc);
struct Anchor* searchURLLabel(struct Buffer* buf, const char* url);
void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos);
const char* reAnchor(struct Buffer* buf, const char* re);
struct AnchorList;
const char* getAnchorText(struct Buffer* buf, struct AnchorList* al, struct Anchor* a);
struct BufferPoint getBufferPosition(struct Buffer* buf);

struct Buffer* makeBuffer(struct UI ui, struct Content* c);
void tmpClearBuffer(struct Buffer* buf);
void shiftvisualpos(struct Buffer* buf, int shift);
void _nextA(struct UI ui, int visited);
void _prevA(struct UI ui, int visited);
