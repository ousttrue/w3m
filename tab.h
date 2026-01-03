#pragma once

struct TabBuffer {
    struct TabBuffer* nextTab;
    struct TabBuffer* prevTab;
    struct Buffer* currentBuffer;
    struct Buffer* firstBuffer;
    short x1;
    short x2;
    short y;
};

// tablist
void _newT(void);
struct TabBuffer* newTab(void);
void calcTabPos(void);
struct TabBuffer* deleteTab(struct TabBuffer* tab);

// a tab
void tab_push_buffer(struct TabBuffer*tab, struct Buffer* buf);
