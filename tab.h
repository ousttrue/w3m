#pragma once

typedef struct _TabBuffer {
    struct _TabBuffer* nextTab;
    struct _TabBuffer* prevTab;
    struct Buffer* currentBuffer;
    struct Buffer* firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

extern TabBuffer* CurrentTab;
extern TabBuffer* FirstTab;
extern TabBuffer* LastTab;
#define NO_TABBUFFER ((TabBuffer*)1)

TabBuffer* newTab(void);
void calcTabPos(void);
TabBuffer* deleteTab(TabBuffer* tab);
