#include "defun.h"
#include "w3m_rc.h"
#include "myctype.h"
#include "func.h"
#include "buffer.h"
#include "document.h"
#include "screen.h"
#include "search.h"

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{
}

DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    tty_clear();
    screen_clear();
    doc_arrangeCursor(&ctx.buf->doc);
}

DEFUN(escmap, ESCMAP, "ESC map")
{
    int c = getch();
    if (IS_ASCII(c))
        escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map")
{
    int c = getch();
    if (IS_DIGIT(c)) {
        escdmap(c);
        return;
    }
    if (IS_ASCII(c))
        escKeyProc((int)c, K_ESCB, EscBKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap")
{
    char c = getch();
    if (IS_ASCII(c)) {
        getRuntime()->CurrentKey = K_MULTI | (getRuntime()->CurrentKey << 16) | c;
        escKeyProc((int)c, 0, NULL);
    }
}

//
// cursor, scroll
//
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    if (getRuntime()->vi_prec_num)
        doc_nscroll(&ctx.buf->doc, searchKeyNum() * (ctx.buf->doc.LINES - 1));
    else
        doc_nscroll(&ctx.buf->doc, getRuntime()->prec_num ? searchKeyNum() : searchKeyNum() * (ctx.buf->doc.LINES - 1));
}

DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    if (getRuntime()->vi_prec_num)
        doc_nscroll(&ctx.buf->doc, -searchKeyNum() * (ctx.buf->doc.LINES - 1));
    else
        doc_nscroll(&ctx.buf->doc, -(getRuntime()->prec_num ? searchKeyNum() : searchKeyNum() * (ctx.buf->doc.LINES - 1)));
}

DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    doc_nscroll(&ctx.buf->doc, searchKeyNum() * (ctx.buf->doc.LINES / 2 - 1));
}

DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    doc_nscroll(&ctx.buf->doc, -searchKeyNum() * (ctx.buf->doc.LINES / 2 - 1));
}

DEFUN(lup1, UP, "Scroll the screen up one line")
{
    doc_nscroll(&ctx.buf->doc, searchKeyNum());
}

DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    doc_nscroll(&ctx.buf->doc, -searchKeyNum());
}

DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    if (!ctx.buf->doc.firstLine)
        return;
    int offsety = /*ctx.buf->doc.LINES / 2*/ -ctx.buf->doc.cursorY;
    if (offsety != 0) {
        ctx.buf->doc.topLine = doc_lineSkip(&ctx.buf->doc, ctx.buf->doc.topLine, -offsety);
        doc_arrangeLine(&ctx.buf->doc);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    if (!ctx.buf->doc.firstLine)
        return;
    int offsetx = ctx.buf->doc.cursorX - ctx.buf->doc.COLS / 2;
    if (offsetx != 0) {
        doc_columnSkip(&ctx.buf->doc, offsetx);
        doc_arrangeCursor(&ctx.buf->doc);
    }
}

DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    if (!ctx.buf->doc.firstLine)
        return;
    int column = ctx.buf->doc.currentColumn;
    doc_columnSkip(&ctx.buf->doc, searchKeyNum() * (-ctx.buf->doc.COLS + 1) + 1);
    doc_shiftvisualpos(&ctx.buf->doc, ctx.buf->doc.currentColumn - column);
}

DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    if (ctx.buf->doc.firstLine == NULL)
        return;
    int column = ctx.buf->doc.currentColumn;
    doc_columnSkip(&ctx.buf->doc, searchKeyNum() * (ctx.buf->doc.COLS - 1) - 1);
    doc_shiftvisualpos(&ctx.buf->doc, ctx.buf->doc.currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct Line* l = ctx.buf->doc.currentLine;
    if (l == NULL)
        return;
    int n = searchKeyNum();
    for (int j = 0; j < n; j++) {
        int column = ctx.buf->doc.currentColumn;
        doc_columnSkip(&ctx.buf->doc, 1);
        if (column == ctx.buf->doc.currentColumn)
            break;
        doc_shiftvisualpos(&ctx.buf->doc, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    struct Line* l = ctx.buf->doc.currentLine;
    if (l == NULL)
        return;
    int n = searchKeyNum();
    for (int j = 0; j < n; j++) {
        if (ctx.buf->doc.currentColumn == 0)
            break;
        doc_columnSkip(&ctx.buf->doc, -1);
        doc_shiftvisualpos(&ctx.buf->doc, -1);
    }
}

//
// search
//

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    srch(&ctx.buf->doc, forwardSearch, "Forward: ");
}

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(&ctx.buf->doc, backwardSearch, "Backward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(&ctx.buf->doc, forwardSearch, "I-search: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(&ctx.buf->doc, backwardSearch, "I-search backward: ");
}

DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward")
{
    srch_nxtprv(&ctx.buf->doc, 0);
}

DEFUN(srchprv, SEARCH_PREV, "Continue search backward")
{
    srch_nxtprv(&ctx.buf->doc, 1);
}
