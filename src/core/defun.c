#include "HttpRequest.h"
#include "util.h"
#include "form.h"
#include "http_message.h"
#include "mailcap.h"
#include "buffer_list.h"
#include "defun_macro.h"
#include "geometry.h" // IWYU pragma: keep
#include "buffer.h"
#include "history.h"
#include "image.h"
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
#include <unistd.h>
#include <wtf.h>
#include <ucs.h>

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

DEFUN(movL, MOVE_LEFT, "Cursor left")
{
    cursorLeft(1);
}

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide")
{
    cursorLeft(1);
}

DEFUN(movD, MOVE_DOWN, "Cursor down")
{
    cursorDown(1);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide")
{
    cursorDown(1);
}

DEFUN(movU, MOVE_UP, "Cursor up")
{
    cursorUp(1);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide")
{
    cursorUp(1);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right")
{
    cursorRight(1);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide")
{
    cursorRight(1);
}

static int
next_nonnull_line(struct UI ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->next)
        ;

    if (l == NULL || l->l.len == 0)
        return -1;

    ui.current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui.current_buffer->pos = 0;
    return 0;
}

static wc_uint32
getChar(const char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

DEFUN(movRW, NEXT_WORD, "Move to the next word")
{
    int n = ui.searchkey_num;
    for (int i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui.current_buffer->document);
        int ppos = ui.current_buffer->pos;

        if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            return;

        struct LineList* l = currentLine(&ui.current_buffer->document);
        const char* lb = l->l.lineBuf;
        while (ui.current_buffer->pos < l->l.len && wc_is_ucs_alnum(getChar(&lb[ui.current_buffer->pos])))
            ui.current_buffer->pos = nextChar(ui.current_buffer->pos, &l->l);

        while (1) {
            while (ui.current_buffer->pos < l->l.len && !wc_is_ucs_alnum(getChar(&lb[ui.current_buffer->pos])))
                ui.current_buffer->pos = nextChar(ui.current_buffer->pos, &l->l);
            if (ui.current_buffer->pos < l->l.len)
                break;
            if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)->next) < 0) {
                ui.current_buffer->document.currentLineIndex = pline->linenumber;
                ui.current_buffer->pos = ppos;
                return;
            }
            ui.current_buffer->pos = 0;
            l = currentLine(&ui.current_buffer->document);
            lb = l->l.lineBuf;
        }
    }
}

static int
prev_nonnull_line(struct UI ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->prev)
        ;
    if (l == NULL || l->l.len == 0)
        return -1;

    ui.current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len;
    return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    int n = ui.searchkey_num;
    for (int i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui.current_buffer->document);
        int ppos = ui.current_buffer->pos;

        if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            goto end;

        while (1) {
            struct LineList* l = currentLine(&ui.current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui.current_buffer->pos > 0) {
                int tmp = prevChar(ui.current_buffer->pos, &l->l);
                if (wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui.current_buffer->pos = tmp;
            }
            if (ui.current_buffer->pos > 0)
                break;
            if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)->prev) < 0) {
                ui.current_buffer->document.currentLineIndex = pline->linenumber;
                ui.current_buffer->pos = ppos;
                goto end;
            }
            ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len;
        }

        {
            struct LineList* l = currentLine(&ui.current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui.current_buffer->pos > 0) {
                int tmp = prevChar(ui.current_buffer->pos, &l->l);
                if (!wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui.current_buffer->pos = tmp;
            }
        }
    }
end:
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit without confirmation")
{
    _quitfm(false);
}

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request")
{
    _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel")
{
    struct Buffer* buf;
    int ok;
    char cmd;

    ok = false;
    do {
        buf = selectBuffer(Firstbuf, ui.current_buffer, &cmd);
        switch (cmd) {
        case 'B':
            ok = true;
            break;
        case '\n':
        case ' ':
            setCurrentBuffer(buf);
            ok = true;
            break;
        case 'D':
            delBuffer(ui, buf);
            break;
        case 'q':
            qquitfm(getUI());
            break;
        case 'Q':
            quitfm(getUI());
            break;
        }
    } while (!ok);

    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == ui.current_buffer)
            continue;
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line")
{
    const char* str = searchKeyData();
    if (str)
        _goLine(ui, str);
    else
        _goLine(ui, inputStr(getUI(), "Goto line: ", ""));
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine(ui, "$");
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line")
{
    if (ui.current_buffer->document.firstLine == NULL)
        return;
    while (currentLine(&ui.current_buffer->document)->next
        && currentLine(&ui.current_buffer->document)->next->bpos)
        cursorDown(1);
    ui.current_buffer->pos = currentLine(&ui.current_buffer->document)->l.len - 1;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source")
{
    const char* fn = ui.current_buffer->filename;
    if (fn == NULL
        || (ui.current_buffer->content.cc.content_type == CONTENTTYPE_UNKNOWN && ui.current_buffer->edit == NULL)
        || /* Reading shell */ ui.current_buffer->content.url.scheme != SCM_LOCAL
        || !strcmp(ui.current_buffer->content.url.file, "-") /* file is std input  */
    ) {
        message(getUI(), MSG_ERR, "Can't edit other than local file");
        return;
    }

    Str cmd;
    if (ui.current_buffer->edit)
        cmd = unquote_mailcap(ui.current_buffer->edit, contentTypeStr(ui.current_buffer->content.cc.content_type), fn,
            getHttpHeaderValue(ui.current_buffer->document_header, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn), 1);
    exec_cmd(cmd->ptr);
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document")
{
    const char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        /* FIXME: gettextize? */
        message(getUI(), MSG_ERR, Sprintf("Can't open %s", tmpf)->ptr);
        return;
    }

    saveBuffer(ui.current_buffer, f, true);
    fclose(f);
    exec_cmd(myEditor(Editor, shell_quote(tmpf),
        1
        // cur_real_linenumber(ui.current_buffer)
        )
            ->ptr);
    unlink(tmpf);
}
