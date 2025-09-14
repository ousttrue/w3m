#include "HttpRequest.h"
#include "buffer_list.h"
#include "defun_macro.h"
#include "geometry.h" // IWYU pragma: keep
#include "buffer.h"
#include "history.h"
#include "linein.h"
#include "local_cgi.h"
#include "runtime.h"
#include "search.h"
#include "keymap.h" // IWYU pragma: keep
#include "w3m.h"
#include "quote.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#define HELP_CGI "w3mhelp"

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

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    int column = ui.current_buffer->currentColumn;
    columnSkip(ui.current_buffer, ui.searchkey_num * (-ui.viewport.size.x + 1) + 1);
    shiftvisualpos(ui.current_buffer, ui.current_buffer->currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column = ui.current_buffer->currentColumn;
    columnSkip(ui.current_buffer, ui.searchkey_num * (ui.viewport.size.x - 1) - 1);
    shiftvisualpos(ui.current_buffer, ui.current_buffer->currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (l == NULL)
        return;

    int n = ui.searchkey_num;
    for (int j = 0; j < n; j++) {
        int column = ui.current_buffer->currentColumn;
        columnSkip(ui.current_buffer, 1);
        if (column == ui.current_buffer->currentColumn)
            break;
        shiftvisualpos(ui.current_buffer, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (l == NULL)
        return;
    int n = ui.searchkey_num;
    for (int j = 0; j < n; j++) {
        if (ui.current_buffer->currentColumn == 0)
            break;
        columnSkip(ui.current_buffer, -1);
        shiftvisualpos(ui.current_buffer, -1);
    }
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    // CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist(ui, "Set environ: ", env, TextHist);
        if (env == NULL || *env == '\0') {

            return;
        }
    }

    char* value;
    if ((value = strchr(env, '=')) != NULL && value > env) {
        char* var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output")
{
    // CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist(ui, "(exec shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0') {
        fmTerm();
        printf("\n");
        (void)!system(cmd); /* We do not care about the exit code here! */
        /* FIXME: gettextize? */
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        // getch();
    }
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer")
{
    const char* fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        /* FIXME: gettextize? */
        fn = inputFilenameHist(ui, "(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
        fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {
        return;
    }
    // cmd_loadfile(ui, fn);
    struct Content c = loadGeneralFile(file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    const char* lang = AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    struct Content c = loadGeneralFile(tmp->ptr, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}
