#pragma once
#include "Buffer.h"

extern int nextpage_topline;

#define _INIT_BUFFER_WIDTH (getCols() - (showLineNum ? 6 : 1))
#define INIT_BUFFER_WIDTH ((_INIT_BUFFER_WIDTH > 0) ? _INIT_BUFFER_WIDTH : 0)
#define FOLD_BUFFER_WIDTH (FoldLine ? (INIT_BUFFER_WIDTH + 1) : -1)

struct Buffer* newBuffer();
struct Buffer* nullBuffer(void);
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
// int columnSkip(struct Buffer* buf, int offset);
struct Content cookie_list_panel(struct UI ui);
void reseq_anchor(struct Buffer* buf);

struct Anchor* searchURLLabel(struct Buffer* buf, const char* url);
void reAnchorWord(struct Buffer* buf, struct LineList* l, int spos, int epos);
const char* reAnchor(struct Buffer* buf, const char* re);
struct AnchorList;
const char* getAnchorText(struct Buffer* buf, struct AnchorList* al, struct Anchor* a);

struct Buffer* makeBuffer(struct UI ui, struct Content* c);
void tmpClearBuffer(struct Buffer* buf);
void shiftvisualpos(struct Buffer* buf, int shift);
void _nextA(struct UI ui, int visited);
void _prevA(struct UI ui, int visited);

