#pragma once

struct _Buffer;

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
