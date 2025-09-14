#include "defun_macro.h"
#include "geometry.h" // IWYU pragma: keep
#include "buffer.h"
#include "search.h"

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{ /* do nothing */
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    ui.current_buffer->document.topLineIndex += ui.viewport.size.y;
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    // nscroll(searchKeyNum() * (getScreen()->ROWS - 1));
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    // nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    // nscroll(-searchKeyNum() * (getScreen()->ROWS / 2 - 1));
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line")
{
    ui.current_buffer->document.topLineIndex++;
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    ui.current_buffer->document.topLineIndex--;
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    int offsety = ui.viewport.size.y / 2 - ui.viewport_cursor.y;
    if (offsety) {
        ui.current_buffer->document.topLineIndex = ui.current_buffer->document.topLineIndex - offsety;
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    int offsetx = ui.viewport_cursor.x - ui.viewport.size.x / 2;
    if (offsetx) {
        columnSkip(ui.current_buffer, offsetx);
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    ui.current_buffer->document = (struct Document) {};
}

/* Search regular expression forward */

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    srch(ui, forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(ui, forwardSearch, "I-search: ");
}

/* Search regular expression backward */

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(ui, backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(ui, backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward")
{
    srch_nxtprv(ui, 0);
}

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward")
{
    srch_nxtprv(ui, 1);
}
