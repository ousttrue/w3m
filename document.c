#include "document.h"
#include "message.h"
#include "w3m_rc.h"
#include "history.h"
#include "anchor.h"
#include "url.h"
#include "alloc.h"
#include "LineWriter.h"
#include "screen.h"
#include <math.h>
#include <string.h>
#include <libwc/wtf_len.h>

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

void doc_addnewline(struct Document* doc, const char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines)
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

struct Line* doc_redrawLine(struct Document* doc, struct Line* l, int i, struct Url* base_url)
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
    screen_move((struct Vec2) { .y = i, .x = 0 });
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
    screen_move((struct Vec2) { .y = i, .x = doc->rootX });
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

struct Line* doc_lineSkip(struct Document* doc, struct Line* line, int offset)
{
    struct Line* l = currentLineSkip(line, offset);
    if (!getRuntime()->nextpage_topline)
        for (int i = doc->LINES - 1 - (doc->lastLine->linenumber - l->linenumber);
            i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

void doc_arrangeLine(struct Document* doc)
{
    if (doc->firstLine == NULL)
        return;
    doc->cursorY = doc->currentLine->linenumber - doc->topLine->linenumber;
    int i = columnPos(doc->currentLine, doc->currentColumn + doc->visualpos - doc->currentLine->bwidth);
    int cpos = COLPOS(doc->currentLine, i) - doc->currentColumn;
    if (cpos >= 0) {
        doc->cursorX = cpos;
        doc->pos = i;
    } else if (doc->currentLine->len > i) {
        doc->cursorX = 0;
        doc->pos = i + 1;
    } else {
        doc->cursorX = 0;
        doc->pos = 0;
    }
}

void doc_cursorUpDown(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;
    struct Line* cl = doc->currentLine;
    if ((doc->currentLine = currentLineSkip(cl, n)) == cl)
        return;
    doc_arrangeLine(doc);
}

void doc_cursorUp0(struct Document* doc, int n)
{
    if (doc->cursorY > 0)
        doc_cursorUpDown(doc, -1);
    else {
        doc->topLine = doc_lineSkip(doc, doc->topLine, -n);
        if (doc->currentLine->prev != NULL)
            doc->currentLine = doc->currentLine->prev;
        doc_arrangeLine(doc);
    }
}

void doc_cursorDown0(struct Document* doc, int n)
{
    if (doc->cursorY < doc->LINES - 1)
        doc_cursorUpDown(doc, 1);
    else {
        doc->topLine = doc_lineSkip(doc, doc->topLine, n);
        if (doc->currentLine->next != NULL)
            doc->currentLine = doc->currentLine->next;
        doc_arrangeLine(doc);
    }
}

void doc_gotoLine(struct Document* doc, int linenumber)
{
    if (doc->firstLine == NULL)
        return;

    if (doc->firstLine->linenumber > linenumber) {
        struct Line* l = doc->firstLine;
        char msg[36];
        sprintf(msg, "First line is #%ld", l->linenumber);
        set_delayed_message(msg);
        doc->topLine = doc->currentLine = l;
        return;
    }

    if (doc->lastLine->linenumber < linenumber) {
        struct Line* l = doc->lastLine;
        char msg[36];
        sprintf(msg, "Last line is #%ld", doc->lastLine->linenumber);
        set_delayed_message(msg);
        doc->currentLine = l;
        doc->topLine = doc_lineSkip(doc, doc->currentLine, -(doc->LINES - 1));
        return;
    }

    for (struct Line* l = doc->firstLine; l != NULL; l = l->next) {
        if (l->linenumber >= linenumber) {
            doc->currentLine = l;
            if (linenumber < doc->topLine->linenumber || doc->topLine->linenumber + doc->LINES <= linenumber)
                doc->topLine = doc_lineSkip(doc, l, -(doc->LINES + 1) / 2);
            break;
        }
    }
}

int doc_columnSkip(struct Document* doc, int offset)
{
    int i, maxColumn;
    int column = doc->currentColumn + offset;
    int nlines = doc->LINES + 1;
    struct Line* l;

    maxColumn = 0;
    for (i = 0, l = doc->topLine; i < nlines && l != NULL; i++, l = l->next) {
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
    }
    maxColumn -= doc->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (doc->currentColumn == maxColumn)
        return 0;
    doc->currentColumn = maxColumn;
    return 1;
}

void doc_arrangeCursor(struct Document* doc)
{
    if (doc == NULL || doc->currentLine == NULL)
        return;

    /* Arrange line */
    if (doc->currentLine->linenumber - doc->topLine->linenumber >= doc->LINES
        || doc->currentLine->linenumber < doc->topLine->linenumber) {
        /*
         * doc->topLine = doc->currentLine;
         */
        doc->topLine = doc_lineSkip(doc, doc->currentLine, 0);
    }

    /* Arrange column */
    int col, col2, pos;
    int delta = 1;
    while (doc->pos < 0 && doc->currentLine->prev && doc->currentLine->bpos) {
        pos = doc->pos + doc->currentLine->prev->len;
        doc_cursorUp0(doc, 1);
        doc->pos = pos;
    }
    while (doc->pos >= doc->currentLine->len && doc->currentLine->next && doc->currentLine->next->bpos) {
        pos = doc->pos - doc->currentLine->len;
        doc_cursorDown0(doc, 1);
        doc->pos = pos;
    }
    if (doc->currentLine->len == 0 || doc->pos < 0)
        doc->pos = 0;
    else if (doc->pos >= doc->currentLine->len)
        doc->pos = doc->currentLine->len - 1;
    while (doc->pos > 0 && doc->currentLine->propBuf[doc->pos] & PC_WCHAR2)
        doc->pos--;
    col = COLPOS(doc->currentLine, doc->pos);
    while (doc->pos + delta < doc->currentLine->len && doc->currentLine->propBuf[doc->pos + delta] & PC_WCHAR2)
        delta++;
    col2 = COLPOS(doc->currentLine, doc->pos + delta);
    if (col < doc->currentColumn || col2 > doc->COLS + doc->currentColumn) {
        doc->currentColumn = 0;
        if (col2 > doc->COLS)
            doc_columnSkip(doc, col);
    }
    /* Arrange cursor */
    doc->cursorY = doc->currentLine->linenumber - doc->topLine->linenumber;
    doc->visualpos = doc->currentLine->bwidth + COLPOS(doc->currentLine, doc->pos) - doc->currentColumn;
    doc->cursorX = doc->visualpos - doc->currentLine->bwidth;
}

void doc_cursorHome(struct Document* doc)
{
    doc->visualpos = 0;
    doc->cursorX = doc->cursorY = 0;
}

void doc_cursorLeft(struct Document* doc, int n)
{
    int i, delta = 1, cpos;
    struct Line* l = doc->currentLine;

    if (doc->firstLine == NULL)
        return;
    i = doc->pos;
    Lineprop* p = l->propBuf;
    while (i - delta > 0 && p[i - delta] & PC_WCHAR2)
        delta++;
    if (i >= delta)
        doc->pos = i - delta;
    else if (l->prev && l->bpos) {
        doc_cursorUp0(doc, -1);
        doc->pos = doc->currentLine->len - 1;
        doc_arrangeCursor(doc);
        return;
    } else
        doc->pos = 0;
    cpos = COLPOS(l, doc->pos);
    doc->visualpos = l->bwidth + cpos - doc->currentColumn;
    if (doc->visualpos - l->bwidth < 0 && n) {
        doc_columnSkip(doc,
            -n + doc->visualpos - l->bwidth - (doc->visualpos - l->bwidth) % n);
        doc->visualpos = l->bwidth + cpos - doc->currentColumn;
    }
    doc->cursorX = doc->visualpos - l->bwidth;
}

void doc_cursorRight(struct Document* doc, int n)
{
    int i, delta = 1, cpos, vpos2;
    struct Line* l = doc->currentLine;

    if (doc->firstLine == NULL)
        return;
    if (doc->pos == l->len && !(l->next && l->next->bpos))
        return;
    i = doc->pos;
    Lineprop* p = l->propBuf;
    while (i + delta < l->len && p[i + delta] & PC_WCHAR2)
        delta++;
    if (i + delta < l->len) {
        doc->pos = i + delta;
    } else if (l->len == 0) {
        doc->pos = 0;
    } else if (l->next && l->next->bpos) {
        doc_cursorDown0(doc, 1);
        doc->pos = 0;
        doc_arrangeCursor(doc);
        return;
    } else {
        doc->pos = l->len - 1;
        while (doc->pos && p[doc->pos] & PC_WCHAR2)
            doc->pos--;
    }
    cpos = COLPOS(l, doc->pos);

    doc->visualpos = l->bwidth + cpos - doc->currentColumn;
    delta = 1;
    while (doc->pos + delta < l->len && p[doc->pos + delta] & PC_WCHAR2)
        delta++;
    vpos2 = COLPOS(l, doc->pos + delta) - doc->currentColumn - 1;
    if (vpos2 >= doc->COLS && n) {
        doc_columnSkip(doc, n + (vpos2 - doc->COLS) - (vpos2 - doc->COLS) % n);
        doc->visualpos = l->bwidth + cpos - doc->currentColumn;
    }
    doc->cursorX = doc->visualpos - l->bwidth;
}

void doc_cursorDown(struct Document* doc, int n)
{
    struct Line* l = doc->currentLine;
    if (doc->firstLine == NULL)
        return;
    while (doc->currentLine->next && doc->currentLine->next->bpos)
        doc_cursorDown0(doc, n);
    if (doc->currentLine == doc->lastLine) {
        doc_gotoLine(doc, l->linenumber);
        doc_arrangeLine(doc);
        return;
    }
    doc_cursorDown0(doc, n);
    while (doc->currentLine->next
        && doc->currentLine->next->bpos
        && doc->currentLine->bwidth + doc->currentLine->width < doc->currentColumn + doc->visualpos)
        doc_cursorDown0(doc, n);
}

void doc_cursorUp(struct Document* doc, int n)
{
    struct Line* l = doc->currentLine;
    if (doc->firstLine == NULL)
        return;
    while (doc->currentLine->prev && doc->currentLine->bpos)
        doc_cursorUp0(doc, n);
    if (doc->currentLine == doc->firstLine) {
        doc_gotoLine(doc, l->linenumber);
        doc_arrangeLine(doc);
        return;
    }
    doc_cursorUp0(doc, n);
    while (doc->currentLine->prev && doc->currentLine->bpos && doc->currentLine->bwidth >= doc->currentColumn + doc->visualpos)
        doc_cursorUp0(doc, n);
}

void doc_cursorXY(struct Document* doc, int x, int y)
{
    doc_cursorUpDown(doc, y - doc->cursorY);

    if (doc->cursorX > x) {
        while (doc->cursorX > x)
            doc_cursorLeft(doc, doc->COLS / 2);
    } else if (doc->cursorX < x) {
        while (doc->cursorX < x) {
            int oldX = doc->cursorX;

            doc_cursorRight(doc, doc->COLS / 2);

            if (oldX == doc->cursorX)
                break;
        }
        if (doc->cursorX > x)
            doc_cursorLeft(doc, doc->COLS / 2);
    }
}

void doc_restorePosition(struct Document* doc, struct Document* orig)
{
    doc->topLine = doc_lineSkip(doc, doc->firstLine, TOP_LINENUMBER(orig) - 1);
    doc_gotoLine(doc, CUR_LINENUMBER(orig));
    doc->pos = orig->pos;
    if (doc->currentLine && orig->currentLine)
        doc->pos += orig->currentLine->bpos - doc->currentLine->bpos;
    doc->currentColumn = orig->currentColumn;
    doc_arrangeCursor(doc);
}

void doc_gotoRealLine(struct Document* doc, int n)
{
    char msg[36];
    struct Line* l = doc->firstLine;

    if (l == NULL)
        return;

    if (l->real_linenumber > n) {
        sprintf(msg, "First line is #%ld", l->real_linenumber);
        set_delayed_message(msg);
        doc->topLine = doc->currentLine = l;
        return;
    }
    if (doc->lastLine->real_linenumber < n) {
        l = doc->lastLine;
        sprintf(msg, "Last line is #%ld", doc->lastLine->real_linenumber);
        set_delayed_message(msg);
        doc->currentLine = l;
        doc->topLine = doc_lineSkip(doc, doc->currentLine, -(doc->LINES - 1));
        return;
    }
    for (; l != NULL; l = l->next) {
        if (l->real_linenumber >= n) {
            doc->currentLine = l;
            if (n < doc->topLine->real_linenumber || doc->topLine->real_linenumber + doc->LINES <= n)
                doc->topLine = doc_lineSkip(doc, l, -(doc->LINES + 1) / 2);
            break;
        }
    }
}

void doc_nscroll(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;

    struct Line* top = doc->topLine;
    struct Line* cur = doc->currentLine;

    int lnum = cur->linenumber;
    doc->topLine = doc_lineSkip(doc, top, n);
    if (doc->topLine == top) {
        lnum += n;
        if (lnum < doc->topLine->linenumber)
            lnum = doc->topLine->linenumber;
        else if (lnum > doc->lastLine->linenumber)
            lnum = doc->lastLine->linenumber;
    } else {
        int tlnum = doc->topLine->linenumber;
        int llnum = doc->topLine->linenumber + doc->LINES - 1;
        int diff_n;
        if (getRuntime()->nextpage_topline)
            diff_n = 0;
        else
            diff_n = n - (tlnum - top->linenumber);
        if (lnum < tlnum)
            lnum = tlnum + diff_n;
        if (lnum > llnum)
            lnum = llnum + diff_n;
    }
    doc_gotoLine(doc, lnum);
    doc_arrangeLine(doc);
    if (n > 0) {
        if (doc->currentLine->bpos && doc->currentLine->bwidth >= doc->currentColumn + doc->visualpos)
            doc_cursorDown(doc, 1);
        else {
            while (doc->currentLine->next && doc->currentLine->next->bpos && doc->currentLine->bwidth + doc->currentLine->width < doc->currentColumn + doc->visualpos)
                doc_cursorDown0(doc, 1);
        }
    } else {
        if (doc->currentLine->bwidth + doc->currentLine->width < doc->currentColumn + doc->visualpos)
            doc_cursorUp(doc, 1);
        else {
            while (doc->currentLine->prev && doc->currentLine->bpos && doc->currentLine->bwidth >= doc->currentColumn + doc->visualpos)
                doc_cursorUp0(doc, 1);
        }
    }
}

void doc_shiftvisualpos(struct Document* doc, int shift)
{
    struct Line* l = doc->currentLine;
    doc->visualpos -= shift;
    if (doc->visualpos - l->bwidth >= doc->COLS)
        doc->visualpos = l->bwidth + doc->COLS - 1;
    else if (doc->visualpos - l->bwidth < 0)
        doc->visualpos = l->bwidth;
    doc_arrangeLine(doc);
    if (doc->visualpos - l->bwidth == -shift && doc->cursorX == 0)
        doc->visualpos = l->bwidth;
}

void doc_movL(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;
    int m = searchKeyNum();
    for (int i = 0; i < m; i++)
        doc_cursorLeft(doc, n);
}

void doc_movR(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;
    int m = searchKeyNum();
    for (int i = 0; i < m; i++)
        doc_cursorRight(doc, n);
}

void doc_movD(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;
    int m = searchKeyNum();
    for (int i = 0; i < m; i++)
        doc_cursorDown(doc, n);
}

void doc_movU(struct Document* doc, int n)
{
    if (doc->firstLine == NULL)
        return;
    int m = searchKeyNum();
    for (int i = 0; i < m; i++)
        doc_cursorUp(doc, n);
}
