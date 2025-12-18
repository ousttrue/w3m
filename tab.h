#pragma once

struct TabBuffer {
    struct TabBuffer* nextTab;
    struct TabBuffer* prevTab;
    struct _Buffer* currentBuffer;
    struct _Buffer* firstBuffer;
    short x1;
    short x2;
    short y;
};

void _newT(void);
struct TabBuffer* newTab(void);
void calcTabPos(void);
struct TabBuffer* deleteTab(struct TabBuffer* tab);
void pushBuffer(struct _Buffer* buf);
