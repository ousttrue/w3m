#include "tab_list.h"
#include "tab.h"
#include "buffer.h"
#include "w3m_rc.h"

struct TabBuffer* tabs_append(struct Buffer* buf)
{
    struct TabBuffer* tab = tab_new();
    tab->firstBuffer = tab->currentBuffer = buf;
    if (g_runtime.CurrentTab) {
        tab->nextTab = g_runtime.CurrentTab->nextTab;
        tab->prevTab = g_runtime.CurrentTab;
        if (g_runtime.CurrentTab->nextTab)
            g_runtime.CurrentTab->nextTab->prevTab = tab;
        else
            g_runtime.LastTab = tab;
        g_runtime.CurrentTab->nextTab = tab;
        g_runtime.CurrentTab = tab;
    } else {
        g_runtime.CurrentTab = tab;
        g_runtime.FirstTab = tab;
        g_runtime.LastTab = tab;
    }
    g_runtime.nTab++;
    return tab;
}

struct TabBuffer*
tabs_delete(struct TabBuffer* tab)
{
    if (nTab() <= 1)
        return FirstTab();

    if (tab->prevTab) {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            getRuntime()->LastTab = tab->prevTab;
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == CurrentTab())
            getRuntime()->CurrentTab = tab->prevTab;
    } else { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        getRuntime()->FirstTab = tab->nextTab;
        if (tab == CurrentTab())
            getRuntime()->CurrentTab = tab->nextTab;
    }
    getRuntime()->nTab--;

    struct Buffer* buf = tab->firstBuffer;
    while (buf) {
        struct Buffer* next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return FirstTab();
}
void tabs_calcPos(int cols)
{
    if (nTab <= 0)
        return;

    int lcol = 0, rcol = 0;
    int n1 = (cols - rcol - lcol) / g_runtime.TabCols;
    int n2, ny;
    if (n1 >= g_runtime.nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = cols / g_runtime.TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_runtime.nTab - n1 - 1) / n2 + 2;
    }

    int na = n1 + n2 * (ny - 1);
    n1 -= (na - g_runtime.nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);

    struct TabBuffer* tab = g_runtime.FirstTab;
    for (int iy = 0; iy < ny && tab; iy++) {
        int nx;
        int col;
        if (iy == 0) {
            nx = n1;
            col = TTY_COLS() - rcol - lcol;
        } else {
            nx = n2 - (na - g_runtime.nTab + (iy - 1)) / (ny - 1);
            col = TTY_COLS();
        }
        for (int ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
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
