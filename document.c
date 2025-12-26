#include "document.h"
#include "alloc.h"
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
