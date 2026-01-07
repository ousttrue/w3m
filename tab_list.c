#include "tab_list.h"
#include "tab.h"
#include "buffer.h"
#include "alloc.h"

static struct TabBuffer* g_CurrentTab = NULL;
static struct TabBuffer* g_FirstTab = NULL;
static struct TabBuffer* g_LastTab = NULL;
static int g_nTab = 0;
static int g_TabCols = 10;

struct TabBuffer* CurrentTab()
{
    return g_CurrentTab;
}
struct TabBuffer* FirstTab()
{
    return g_FirstTab;
}
struct TabBuffer* LastTab()

{
    return g_LastTab;
}
struct TabBuffer* numTab(int n)
{
    if (n == 0)
        return CurrentTab();
    if (n == 1)
        return FirstTab();
    if (nTab() <= 1)
        return NULL;
    struct TabBuffer* tab = FirstTab();
    for (int i = 1; tab && i < n; tab = tab->nextTab, i++)
        ;
    return tab;
}

int nTab()
{
    return g_nTab;
}

// void tabs_prepare()
// {
//     g_runtime.CurrentTab = g_runtime.LastTab;
//     if (!g_runtime.FirstTab) {
//         g_runtime.FirstTab = g_runtime.LastTab = g_runtime.CurrentTab = tab_new();
//         g_runtime.nTab = 1;
//     }
// }

size_t tabs_current()
{
    int i = 0;
    for (struct TabBuffer* tab = g_FirstTab; tab; tab = tab->nextTab, ++i) {
        if (tab == g_CurrentTab) {
            return i;
        }
    }
    return -1;
}

struct TabBuffer* tabs_tab(size_t index)
{
    int i = 0;
    for (struct TabBuffer* tab = g_FirstTab; tab; tab = tab->nextTab, ++i) {
        if (i == index) {
            return tab;
        }
    }
    return NULL;
}

struct TabBuffer* tabs_append(struct Buffer* buf)
{
    struct TabBuffer* tab = tab_new();
    tab->firstBuffer = tab->currentBuffer = buf;
    if (g_CurrentTab) {
        tab->nextTab = g_CurrentTab->nextTab;
        tab->prevTab = g_CurrentTab;
        if (g_CurrentTab->nextTab)
            g_CurrentTab->nextTab->prevTab = tab;
        else
            g_LastTab = tab;
        g_CurrentTab->nextTab = tab;
        g_CurrentTab = tab;
    } else {
        g_CurrentTab = tab;
        g_FirstTab = tab;
        g_LastTab = tab;
    }
    g_nTab++;
    return tab;
}

struct TabBuffer*
tabs_delete(struct TabBuffer* tab)
{
    if (g_nTab <= 1)
        return g_FirstTab;

    if (tab->prevTab) {
        if (tab->nextTab)
            tab->nextTab->prevTab = tab->prevTab;
        else
            g_LastTab = tab->prevTab;
        tab->prevTab->nextTab = tab->nextTab;
        if (tab == g_CurrentTab)
            g_CurrentTab = tab->prevTab;
    } else { /* tab == FirstTab */
        tab->nextTab->prevTab = NULL;
        g_FirstTab = tab->nextTab;
        if (tab == g_CurrentTab)
            g_CurrentTab = tab->nextTab;
    }
    g_nTab--;

    struct Buffer* buf = tab->firstBuffer;
    while (buf) {
        struct Buffer* next = buf->nextBuffer;
        discardBuffer(buf);
        buf = next;
    }
    return g_FirstTab;
}

struct TabPosList tabs_calcPos(int cols)
{
    if (g_nTab <= 0)
        return (struct TabPosList) { 0 };

    int lcol = 0, rcol = 0;
    int n1 = (cols - rcol - lcol) / g_TabCols;
    int n2, ny;
    if (n1 >= g_nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = cols / g_TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_nTab - n1 - 1) / n2 + 2;
    }

    int na = n1 + n2 * (ny - 1);
    n1 -= (na - g_nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);

    struct TabPosList tabpos = {
        .data = New_N(struct TabPos, g_nTab),
        .len = g_nTab,
    };
    struct TabBuffer* tab = g_FirstTab;
    int i = 0;
    for (int iy = 0; iy < ny && tab; iy++) {
        int nx;
        int col;
        if (iy == 0) {
            nx = n1;
            col = cols - rcol - lcol;
        } else {
            nx = n2 - (na - g_nTab + (iy - 1)) / (ny - 1);
            col = cols;
        }
        for (int ix = 0; ix < nx && tab; ix++, tab = tab->nextTab, ++i) {
            struct TabPos* p = &tabpos.data[i];
            p->x1 = col * ix / nx;
            p->x2 = col * (ix + 1) / nx - 1;
            p->y = iy;
            if (iy == 0) {
                p->x1 += lcol;
                p->x2 += lcol;
            }
        }
    }
    return tabpos;
}

#define NO_TABBUFFER ((struct TabBuffer*)1)

void moveTab(struct TabBuffer* t, struct TabBuffer* t2, int right)
{
    if (t2 == NO_TABBUFFER)
        t2 = g_FirstTab;
    if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
        return;
    if (t->prevTab) {
        if (t->nextTab)
            t->nextTab->prevTab = t->prevTab;
        else
            g_LastTab = t->prevTab;
        t->prevTab->nextTab = t->nextTab;
    } else {
        t->nextTab->prevTab = NULL;
        g_FirstTab = t->nextTab;
    }
    if (right) {
        t->nextTab = t2->nextTab;
        t->prevTab = t2;
        if (t2->nextTab)
            t2->nextTab->prevTab = t;
        else
            g_LastTab = t;
        t2->nextTab = t;
    } else {
        t->prevTab = t2->prevTab;
        t->nextTab = t2;
        if (t2->prevTab)
            t2->prevTab->nextTab = t;
        else
            g_FirstTab = t;
        t2->prevTab = t;
    }
}

void tabs_set_current(struct TabBuffer* tab)
{
    g_CurrentTab = tab;
    for (tab = g_LastTab; tab != NULL; tab = tab->prevTab) {
        if (tab == g_CurrentTab)
            continue;
        struct Buffer* buf = tab->currentBuffer;
        deleteImage(buf);
        // if (getRuntime()->clear_buffer)
        //     tmpClearBuffer(buf);
    }
}

void tabs_next(int n)
{
    if (g_nTab <= 1)
        return;
    for (int i = 0; i < n; i++) {
        if (g_CurrentTab->nextTab)
            g_CurrentTab = g_CurrentTab->nextTab;
        else
            g_CurrentTab = g_FirstTab;
    }
}

void tabs_prev(int n)
{
    if (g_nTab <= 1)
        return;
    for (int i = 0; i < n; i++) {
        if (g_CurrentTab->prevTab)
            g_CurrentTab = g_CurrentTab->prevTab;
        else
            g_CurrentTab = g_LastTab;
    }
}
