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

void tab_repBuffer(struct TabBuffer* tab, struct Buffer* oldbuf, struct Buffer* buf)
{
    tab->firstBuffer = replaceBuffer(tab->firstBuffer, oldbuf, buf);
    tab->currentBuffer = buf;
}

void tab_delBuffer(struct TabBuffer* tab, struct Buffer* buf)
{
    if (!tab)
        return;
    if (!buf)
        return;
    if (tab->currentBuffer == buf) {
        tab->currentBuffer = buf->nextBuffer;
    }
    tab_deleteBuffer(tab, buf);
    if (!tab->currentBuffer) {
        tab->currentBuffer = tab->firstBuffer;
    }
}

bool tab_currentBufferSubmit(struct TabBuffer* tab)
{
    if (!tab->currentBuffer->doc) {
        return false;
    }
    struct Anchor* a = tab->currentBuffer->doc->submit;
    if (!a) {
        return false;
    }
    tab->currentBuffer->doc->submit = NULL;
    doc_gotoLine(tab->currentBuffer->doc, a->start.line);
    tab->currentBuffer->doc->pos = a->start.pos;
    struct FollowResult result = buf_followForm(
        tab->currentBuffer,
        (struct FollowOption) { .on_target = true, .do_download = false }, true);
    if (result.new_buf) {
        tab_push_buffer(tab, result.new_buf);
    }
    return true;
}

void tab_back(struct TabBuffer* tab)
{
    if (!checkBackBuffer(tab->currentBuffer)) {
        // if (getRuntime()->close_tab_back && nTab() >= 1) {
        //     tabs_delete(ctx.tab);
        // } else {
        // disp_message("Can't go back...", TRUE);
        // }
        return;
    }

    tab_delBuffer(tab, tab->currentBuffer);
}

void tab_deleteBuffer(struct TabBuffer* tab, struct Buffer* delbuf)
{
    if (tab->firstBuffer == delbuf && tab->firstBuffer->nextBuffer) {
        struct Buffer* buf = tab->firstBuffer->nextBuffer;
        buf_discard(tab->firstBuffer);
        tab->firstBuffer = buf;
        return;
    }

    struct Buffer* buf = prevBuffer(tab->firstBuffer, delbuf);
    if (buf) {
        struct Buffer* b = buf->nextBuffer;
        buf->nextBuffer = b->nextBuffer;
        buf_discard(b);
    }
}
