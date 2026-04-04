#pragma once
#include <stdio.h>
#include "tab.h"

typedef struct _Buffer Buffer;

/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

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
extern Buffer* getshell(char* cmd);
extern Buffer* getpipe(char* cmd);
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
