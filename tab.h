#ifndef W3M_TAB_H
#define W3M_TAB_H
#include "buffer.h"

typedef struct _TabBuffer {
    struct _TabBuffer *nextTab;
    struct _TabBuffer *prevTab;
    Buffer *currentBuffer;
    Buffer *firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

extern int nTab;
extern int TabCols;
extern TabBuffer *CurrentTab;
extern TabBuffer *FirstTab;
extern TabBuffer *LastTab;

extern TabBuffer *deleteTab(TabBuffer * tab);

#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)
#define NO_TABBUFFER ((TabBuffer *)1)
#endif
