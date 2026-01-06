#pragma once

struct TabBuffer {
    struct TabBuffer* nextTab;
    struct TabBuffer* prevTab;
    struct Buffer* currentBuffer;
    struct Buffer* firstBuffer;
};

struct TabBuffer* tab_new(void);
void tab_push_buffer(struct TabBuffer* tab, struct Buffer* buf);
