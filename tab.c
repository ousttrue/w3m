#include "tab.h"
#include "w3m_runtime.h"
#include "image.h"
#include "fm.h"

struct TabBuffer* CurrentTab = 0;
struct TabBuffer* FirstTab = 0;
struct TabBuffer* LastTab = 0;

void _newT(void)
{
    struct TabBuffer* tag = newTab();
    if (!tag)
        return;

    Buffer* buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = CurrentTab->nextTab;
    tag->prevTab = CurrentTab;
    if (CurrentTab->nextTab)
        CurrentTab->nextTab->prevTab = tag;
    else
        LastTab = tag;
    CurrentTab->nextTab = tag;
    CurrentTab = tag;
    nTab++;
}

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

void calcTabPos(void)
{
    struct TabBuffer* tab;
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    if (nTab <= 0)
        return;
    n1 = (TTY_COLS() - rcol - lcol) / TabCols;
    if (n1 >= nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = TTY_COLS() / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
        if (iy == 0) {
            nx = n1;
            col = TTY_COLS() - rcol - lcol;
        } else {
            nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
            col = TTY_COLS();
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;
            tab->y = iy;
            if (iy == 0) {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

void pushBuffer(Buffer* buf)
{
    deleteImage(Currentbuf);
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);

    Buffer* b;
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
