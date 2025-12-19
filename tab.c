#include "tab.h"
#include "buffer.h"
#include "w3m_rc.h"
#include "image.h"
#include "fm.h"

struct TabBuffer* newTab(void)
{
    struct TabBuffer* n = New(struct TabBuffer);
    if (n == NULL)
        return NULL;
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}

void pushBuffer(struct Buffer* buf)
{
    deleteImage(Currentbuf);
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);

    struct Buffer* b;
    if (Firstbuf == Currentbuf) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = Currentbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = Currentbuf;
        Currentbuf = buf;
    }
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif
}
