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
        buf->back = tab->firstBuffer;
        tab->firstBuffer = tab->currentBuffer = buf;
    } else if ((b = tab_prevBuffer(tab, tab->currentBuffer)) != NULL) {
        b->back = buf;
        buf->back = tab->currentBuffer;
        tab->currentBuffer = buf;
    }
}

/*
 * replaceBuffer: replace buffer
 */
struct Buffer*
tab_replaceBuffer(struct TabBuffer* tab, struct Buffer* delbuf, struct Buffer* newbuf)
{
    struct Buffer* buf;

    if (delbuf == NULL) {
        newbuf->back = tab->firstBuffer;
        return newbuf;
    }
    if (tab->firstBuffer == delbuf) {
        newbuf->back = delbuf->back;
        buf_discard(delbuf);
        return newbuf;
    }
    if (delbuf && (buf = tab_prevBuffer(tab, delbuf))) {
        buf->back = newbuf;
        newbuf->back = delbuf->back;
        buf_discard(delbuf);
        return tab->firstBuffer;
    }
    newbuf->back = tab->firstBuffer;
    return newbuf;
}

void tab_repBuffer(struct TabBuffer* tab, struct Buffer* oldbuf, struct Buffer* buf)
{
    tab->firstBuffer = tab_replaceBuffer(tab, oldbuf, buf);
    tab->currentBuffer = buf;
}

void tab_delBuffer(struct TabBuffer* tab, struct Buffer* buf)
{
    if (!tab)
        return;
    if (!buf)
        return;
    if (tab->currentBuffer == buf) {
        tab->currentBuffer = buf->back;
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
    if (!tab->currentBuffer->back) {
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
    if (tab->firstBuffer == delbuf && tab->firstBuffer->back) {
        struct Buffer* buf = tab->firstBuffer->back;
        buf_discard(tab->firstBuffer);
        tab->firstBuffer = buf;
        return;
    }

    struct Buffer* buf = tab_prevBuffer(tab, delbuf);
    if (buf) {
        struct Buffer* b = buf->back;
        buf->back = b->back;
        buf_discard(b);
    }
}

struct Buffer*
tab_nthBuffer(struct TabBuffer* tab, int n)
{
    if (n < 0)
        return tab->firstBuffer;
    struct Buffer* buf = tab->firstBuffer;
    for (int i = 0; i < n; i++) {
        if (buf == NULL)
            return NULL;
        buf = buf->back;
    }
    return buf;
}

struct Buffer*
tab_prevBuffer(struct TabBuffer* tab, struct Buffer* buf)
{
    struct Buffer* b = tab->firstBuffer;
    for (; b != NULL && b->back != buf; b = b->back)
        ;
    return b;
}
