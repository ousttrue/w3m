#include "tab.h"
#include "alloc.h"
#include "buffer.h"
#include "w3m_rc.h"
#include "image.h"

struct TabBuffer* tab_new(void)
{
    struct TabBuffer* tab = New(struct TabBuffer);
    *tab = (struct TabBuffer) {
        .nextTab = NULL,
        .currentBuffer = NULL,
        .firstBuffer = NULL,
    };
    return tab;
}

void tab_push_buffer(struct TabBuffer* tab, struct Buffer* buf)
{
    deleteImage(tab->currentBuffer);
    if (getRuntime()->clear_buffer)
        tmpClearBuffer(tab->currentBuffer);

    struct Buffer* b;
    if (tab->firstBuffer == tab->currentBuffer) {
        buf->nextBuffer = tab->firstBuffer;
        tab->firstBuffer = tab->currentBuffer = buf;
    } else if ((b = prevBuffer(tab->firstBuffer, tab->currentBuffer)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = tab->currentBuffer;
        tab->currentBuffer = buf;
    }
}
