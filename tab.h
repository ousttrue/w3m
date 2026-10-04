#pragma once
#include "buffer.h"

typedef struct _TabBuffer {
    struct _TabBuffer* nextTab;
    struct _TabBuffer* prevTab;
    Buffer* currentBuffer;
    Buffer* firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

extern int nTab;
extern int TabCols;
extern TabBuffer* CurrentTab;
extern TabBuffer* FirstTab;
extern TabBuffer* LastTab;

#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)
#define NO_TABBUFFER ((TabBuffer*)1)
#define NO_BUFFER ((Buffer*)1)

void pushBuffer(Buffer* buf);
TabBuffer* deleteTab(TabBuffer* tab);
