#include "document.h"
#include "w3m_rc.h"
#include "history.h"
#include "anchor.h"
#include "url.h"
#include "alloc.h"
#include "LineWriter.h"
#include "terms.h"
#include <math.h>
#include <string.h>

extern char* NullLine;
extern Lineprop NullProp[];

static void
addnewline2(struct Document* doc, char* line, Lineprop* prop, Linecolor* color, int pos, int nlines)
{
    struct Line* l;
    l = New(struct Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
    l->colorBuf = color;
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = doc->currentLine;
    if (doc->currentLine) {
        l->next = doc->currentLine->next;
        doc->currentLine->next = l;
    } else {
        l->next = NULL;
    }
    if (doc->lastLine == NULL || doc->lastLine == doc->currentLine)
        doc->lastLine = l;
    doc->currentLine = l;
    if (doc->firstLine == NULL)
        doc->firstLine = l;
    l->linenumber = ++doc->allLine;
    if (nlines < 0) {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    } else {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

void addnewline(struct Document* doc, const char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines)
{
    char* s;
    Lineprop* p;
    if (pos > 0) {
        s = allocStr(line, pos);
        p = NewAtom_N(Lineprop, pos);
        memcpy(p, prop, pos * sizeof(Lineprop));
    } else {
        s = NullLine;
        p = NullProp;
    }

    Linecolor* c;
    if (pos > 0 && color) {
        c = NewAtom_N(Linecolor, pos);
        bcopy((void*)color, (void*)c, pos * sizeof(Linecolor));
    } else {
        c = NULL;
    }

    addnewline2(doc, s, p, c, pos, nlines);
    if (pos <= 0 || width <= 0)
        return;

    int bpos = 0;
    int bwidth = 0;
    while (1) {
        struct Line* l = doc->currentLine;
        l->bpos = bpos;
        l->bwidth = bwidth;
        int i = columnLen(l, width);
        if (i == 0) {
            i++;
            while (i < l->len && p[i] & PC_WCHAR2)
                i++;
        }
        l->len = i;
        l->width = COLPOS(l, l->len);
        if (pos <= i)
            return;
        bpos += l->len;
        bwidth += l->width;
        s += i;
        p += i;
        if (c)
            c += i;
        pos -= i;
        addnewline2(doc, s, p, c, pos, nlines);
    }
}

struct Line* redrawLine(struct Document* doc, struct Line* l, int i, struct Url* base_url)
{
    struct LineWriter g = { 0 };

    int j, pos, rcol, ncol, delta = 1;
    int column = doc->currentColumn;
    char* p;
    Lineprop* pr;
    Linecolor* pc;
    struct Anchor* a;
    struct Url url;
    int k, vpos = -1;

    if (l == NULL) {
        return NULL;
    }
    screen_move(i, 0);
    if (getRuntime()->showLineNum) {
        char tmp[16];
        if (!doc->rootX) {
            if (doc->lastLine->real_linenumber > 0)
                doc->rootX = (int)(log(doc->lastLine->real_linenumber + 0.1)
                                 / log(10))
                    + 2;
            if (doc->rootX < 5)
                doc->rootX = 5;
            if (doc->rootX > TTY_COLS())
                doc->rootX = TTY_COLS();
            doc->COLS = TTY_COLS() - doc->rootX;
        }
        if (l->real_linenumber && !l->bpos)
            sprintf(tmp, "%*ld:", doc->rootX - 1, l->real_linenumber);
        else
            sprintf(tmp, "%*s ", doc->rootX - 1, "");
        screen_wc_addstr(tmp);
    }
    screen_move(i, doc->rootX);
    if (l->width < 0)
        l->width = COLPOS(l, l->len);
    if (l->len == 0 || l->width - 1 < column) {
        screen_clrtoeolx();
        return l;
    }
    /* need_clrtoeol(); */
    pos = columnPos(l, column);
    p = &(l->lineBuf[pos]);
    pr = &(l->propBuf[pos]);
    if (getRuntime()->useColor && l->colorBuf)
        pc = &(l->colorBuf[pos]);
    else
        pc = NULL;
    rcol = COLPOS(l, pos);

    for (j = 0; rcol - column < doc->COLS && pos + j < l->len; j += delta) {
        if (getRuntime()->useVisitedColor && vpos <= pos + j && !(pr[j] & PE_VISITED)) {
            a = retrieveAnchor(doc->href, l->linenumber, pos + j);
            if (a) {
                parseURL2(a->url, &url, base_url);
                if (getHashHist(getRuntime()->URLHist, parsedURL2Str(&url)->ptr)) {
                    for (k = a->start.pos; k < a->end.pos; k++)
                        pr[k - pos] |= PE_VISITED;
                }
                vpos = a->end.pos;
            }
        }
        delta = wtf_len((wc_uchar*)&p[j]);
        ncol = COLPOS(l, pos + j + delta);
        if (ncol - column > doc->COLS)
            break;
        if (pc)
            do_color(&g, pc[j]);
        if (rcol < column) {
            for (rcol = column; rcol < ncol; rcol++)
                addChar(&g, ' ', 0);
            continue;
        }
        if (p[j] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(&g, ' ', 0);
        } else {
            addMChar(&g, &p[j], pr[j], delta);
        }
        rcol = ncol;
    }
    endLine(&g);
    if (rcol - column < doc->COLS)
        screen_clrtoeolx();
    return l;
}
