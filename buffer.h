#ifndef BUFFER_H_
#define BUFFER_H_

#include "fm.h"

Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
Buffer *newBuffer(int width);
Buffer *nthBuffer(Buffer *firstbuf, int n);
Buffer *nullBuffer(void);
Buffer *prevBuffer(Buffer *first, const Buffer *buf);
Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf, char *selectchar);
char *getCurWord(Buffer *buf, int *spos, int *epos);
char *GetWord(Buffer *buf);
int readBufferCache(Buffer *buf);
int writeBufferCache(Buffer *buf);
void clearBuffer(Buffer *buf);
void copyBuffer(Buffer *a, Buffer *b);
void discardBuffer(Buffer *buf);
void gotoLine(Buffer *buf, int n);
void gotoRealLine(Buffer *buf, int n);
void reshapeBuffer(Buffer *buf);

#endif
