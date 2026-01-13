#pragma once
#include <stdbool.h>

struct TabBuffer {
    struct TabBuffer* nextTab;
    struct TabBuffer* prevTab;
    struct Buffer* currentBuffer;
    struct Buffer* firstBuffer;
};

struct TabBuffer* tab_new(void);
void tab_push_buffer(struct TabBuffer* tab, struct Buffer* buf);
void tab_repBuffer(struct TabBuffer* tab, struct Buffer* oldbuf, struct Buffer* buf);
void tab_delBuffer(struct TabBuffer* tab, struct Buffer* buf);
bool tab_currentBufferSubmit(struct TabBuffer* tab);
void tab_back(struct TabBuffer* tab);
void tab_deleteBuffer(struct TabBuffer* tab, struct Buffer* delbuf);
struct Buffer* tab_replaceBuffer(struct TabBuffer* tab, struct Buffer* delbuf, struct Buffer* newbuf);
struct Buffer* tab_nthBuffer(struct TabBuffer* tab, int n);
