#include "AnchorList.h"
#include "Anchor.h"
#include "HttpRequest.h"
#include "follow_anchor.h"
#include "myctype.h"
#include "regex.h"
#include "util.h"
#include "form.h"
#include "http_message.h"
#include "mailcap.h"
#include "buffer_list.h"
#include "defun_macro.h"
#include "geometry.h" // IWYU pragma: keep
#include "buffer_util.h"
#include "history.h"
#include "image_loader.h"
#include "linein.h"
#include "local_cgi.h"
#include "runtime.h"
#include "search.h"
#include "keymap.h" // IWYU pragma: keep
#include "w3m.h"
#include "quote.h"
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
    ui.document->topLineIndex += ui.viewport.size.y;
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
    ui.document->topLineIndex++;
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    ui.document->topLineIndex--;
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    int offsety = ui.viewport.size.y / 2 - ui.viewport_cursor.y;
    if (offsety) {
        ui.document->topLineIndex = ui.document->topLineIndex - offsety;
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    int offsetx = ui.viewport_cursor.x - ui.viewport.size.x / 2;
    if (offsetx) {
        ui.document->currentColumn += offsetx;
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    // ui.current_buffer->document = 0;
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
    int column = ui.document->currentColumn;
    ui.document->currentColumn += (ui.searchkey_num * (-ui.viewport.size.x + 1) + 1);
    shiftvisualpos(ui.current_buffer, ui.document->currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column = ui.document->currentColumn;
    ui.document->currentColumn += (ui.searchkey_num * (ui.viewport.size.x - 1) - 1);
    shiftvisualpos(ui.current_buffer, ui.document->currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (l == NULL)
        return;

    int n = ui.searchkey_num;
    for (int j = 0; j < n; j++) {
        int column = ui.document->currentColumn;
        ui.document->currentColumn += 1;
        if (column == ui.document->currentColumn)
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
        if (ui.document->currentColumn == 0)
            break;
        ui.document->currentColumn += (-1);
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
    struct Content c = getContent(file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    const char* lang = AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    struct Content c = getContent(tmp->ptr, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
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

    ui.document->currentLineIndex = l->linenumber;
    if (l != line)
        ui.document->pos = 0;
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
        int ppos = ui.document->pos;

        if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            return;

        struct LineList* l = currentLine(&ui.current_buffer->document);
        const char* lb = l->l.lineBuf;
        while (ui.document->pos < l->l.len && wc_is_ucs_alnum(getChar(&lb[ui.document->pos])))
            ui.document->pos = nextChar(ui.document->pos, &l->l);

        while (1) {
            while (ui.document->pos < l->l.len && !wc_is_ucs_alnum(getChar(&lb[ui.document->pos])))
                ui.document->pos = nextChar(ui.document->pos, &l->l);
            if (ui.document->pos < l->l.len)
                break;
            if (next_nonnull_line(ui, currentLine(&ui.current_buffer->document)->next) < 0) {
                ui.document->currentLineIndex = pline->linenumber;
                ui.document->pos = ppos;
                return;
            }
            ui.document->pos = 0;
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

    ui.document->currentLineIndex = l->linenumber;
    if (l != line)
        ui.document->pos = currentLine(&ui.current_buffer->document)->l.len;
    return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    int n = ui.searchkey_num;
    for (int i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui.current_buffer->document);
        int ppos = ui.document->pos;

        if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)) < 0)
            goto end;

        while (1) {
            struct LineList* l = currentLine(&ui.current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui.document->pos > 0) {
                int tmp = prevChar(ui.document->pos, &l->l);
                if (wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui.document->pos = tmp;
            }
            if (ui.document->pos > 0)
                break;
            if (prev_nonnull_line(ui, currentLine(&ui.current_buffer->document)->prev) < 0) {
                ui.document->currentLineIndex = pline->linenumber;
                ui.document->pos = ppos;
                goto end;
            }
            ui.document->pos = currentLine(&ui.current_buffer->document)->l.len;
        }

        {
            struct LineList* l = currentLine(&ui.current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui.document->pos > 0) {
                int tmp = prevChar(ui.document->pos, &l->l);
                if (!wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui.document->pos = tmp;
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
            delBuffer(buf);
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
        deleteImage(&buf->document);
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
    if (ui.document->firstLine == NULL)
        return;
    while (currentLine(&ui.current_buffer->document)->next
        && currentLine(&ui.current_buffer->document)->next->bpos)
        cursorDown(1);
    ui.document->pos = currentLine(&ui.current_buffer->document)->l.len - 1;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source")
{
    // const char* fn = ui.current_buffer->filename;
    // if (fn == NULL
    //     || (ui.current_buffer->content.cc.content_type == CONTENTTYPE_UNKNOWN && ui.current_buffer->edit == NULL)
    //     || /* Reading shell */ ui.current_buffer->content.url.scheme != SCM_LOCAL
    //     || !strcmp(ui.current_buffer->content.url.file, "-") /* file is std input  */
    // ) {
    //     message(getUI(), MSG_ERR, "Can't edit other than local file");
    //     return;
    // }

    Str cmd;
    if (ui.current_buffer->edit)
        cmd = unquote_mailcap(ui.current_buffer->edit,
            contentTypeStr(ui.current_buffer->content.cc.content_type), ui.current_buffer->content.sourcefile,
            getHttpHeaderValue(ui.current_buffer->content.document_header, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(ui.current_buffer->content.sourcefile), 1);
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

/* Set / unset mark */
DEFUN(_mark, MARK, "Set/unset mark")
{
    if (!use_mark)
        return;
    if (ui.document->firstLine == NULL)
        return;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    l->l.propBuf[ui.document->pos] ^= PE_MARK;
}

/* Go to next mark */
DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    if (!use_mark)
        return;
    if (ui.document->firstLine == NULL)
        return;
    int i = ui.document->pos + 1;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (i >= l->l.len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->l.len; i++) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui.document->currentLineIndex = l->linenumber;
                ui.document->pos = i;

                return;
            }
        }
        l = l->next;
        i = 0;
    }
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "No mark exist after here");
}

/* Go to previous mark */
DEFUN(prevMk, PREV_MARK, "Go to the previous mark")
{
    if (!use_mark)
        return;
    if (ui.document->firstLine == NULL)
        return;
    int i = ui.document->pos - 1;
    struct LineList* l = currentLine(&ui.current_buffer->document);
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui.document->currentLineIndex = l->linenumber;
                ui.document->pos = i;

                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    /* FIXME: gettextize? */
    message(getUI(), MSG_INFO, "No mark exist before here");
}

static const char* MarkString = NULL;

/* Mark place to which the regular expression matches */
DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    if (!use_mark)
        return;

    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(getUI(), "(Mark)Regexp: ", MarkString, TextHist);
        if (str == NULL || *str == '\0') {

            return;
        }
    }
    str = conv_search_string(ui, str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
        message(getUI(), MSG_INFO, str);
        return;
    }

    struct LineList* l;
    MarkString = str;
    for (l = ui.document->firstLine; l != NULL; l = l->next) {
        const char* p = l->l.lineBuf;
        for (;;) {
            if (regexMatch(p, &l->l.lineBuf[l->l.len] - p, p == l->l.lineBuf) == 1) {
                const char *p1, *p2;
                matchedPosition(&p1, &p2);
                l->l.propBuf[p1 - l->l.lineBuf] |= PE_MARK;
                p = p2;
            } else
                break;
        }
    }
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    followAnchor(ui, false);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer")
{
    followImage(ui, false);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form")
{
    _followForm(ui, true, false);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink")
{
    struct HmarkerList* hl = ui.document->hmarklist;
    if (ui.document->firstLine == NULL)
        return;

    if (!hl || hl->nmark == 0)
        return;

    struct BufferPoint* po;
    struct Anchor* an;
    int hseq = 0;
    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(ui.document->href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui.document->formitem, *po);
        hseq++;
    } while (an == NULL);

    gotoLine(&ui.current_buffer->document, po->line);
    ui.document->pos = po->pos;
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink")
{
    struct HmarkerList* hl = ui.document->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq;

    if (ui.document->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(ui.document->href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui.document->formitem, *po);
        hseq--;
    } while (an == NULL);

    gotoLine(&ui.current_buffer->document, po->line);
    ui.document->pos = po->pos;
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link")
{
    struct HmarkerList* hl = ui.document->hmarklist;

    int n = ui.searchkey_num;
    if (n < 0 || n > hl->nmark)
        return;

    if (ui.document->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    struct BufferPoint* po = hl->marks + n - 1;
    struct Anchor* an = retrieveAnchor(ui.document->href, *po);
    if (an == NULL)
        an = retrieveAnchor(ui.document->formitem, *po);
    if (an == NULL)
        return;

    gotoLine(&ui.current_buffer->document, po->line);
    ui.document->pos = po->pos;
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink")
{
    _nextA(ui, false);
}

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink")
{
    _prevA(ui, false);
}

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink")
{
    _nextA(ui, true);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink")
{
    _prevA(ui, true);
}

/* go to the next left/right anchor */
static void
nextX(struct UI ui, int d, int dy)
{
    if (ui.document->firstLine == NULL)
        return;

    struct HmarkerList* hl = ui.document->hmarklist;
    if (!hl || hl->nmark == 0)
        return;

    struct Anchor* an = retrieveAnchor(ui.document->href, getBufferPosition(ui));
    if (an == NULL)
        an = retrieveAnchor(ui.document->formitem, getBufferPosition(ui));

    int y;
    struct Anchor* pan = getNextHorizontalAnchor(&ui.current_buffer->document, an, ui.searchkey_num, d, dy);
    if (pan == NULL)
        return;

    ui.document->pos = pan->start.pos;
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink")
{
    nextX(ui, -1, 0);
}

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink")
{
    nextX(ui, -1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink")
{
    nextX(ui, 1, 0);
}

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink")
{
    nextX(ui, 1, 1);
}

/* go to the next downward/upward anchor */
static void
nextY(struct UI ui, int d)
{
    struct HmarkerList* hl = ui.document->hmarklist;
    struct Anchor* pan;
    int i, x, y, n = ui.searchkey_num;
    int hseq;

    if (ui.document->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    struct Anchor* an = retrieveAnchor(ui.document->href, getBufferPosition(ui));
    if (an == NULL)
        an = retrieveAnchor(ui.document->formitem, getBufferPosition(ui));

    x = ui.document->pos;
    y = currentLine(&ui.current_buffer->document)->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++) {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= lastLine(&ui.current_buffer->document)->linenumber; y += d) {
            struct BufferPoint bp = { .line = y, .pos = x };
            an = retrieveAnchor(ui.document->href, bp);
            if (!an)
                an = retrieveAnchor(ui.document->formitem, bp);
            if (an && hseq != abs(an->hseq)) {
                pan = an;
                break;
            }
        }
        if (!an)
            break;
    }

    if (pan == NULL)
        return;
    gotoLine(&ui.current_buffer->document, pan->start.line);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink")
{
    nextY(ui, 1);
}

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink")
{
    nextY(ui, -1);
}

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Switch to the next buffer")
{
    setCurrentBuffer(prevBuffer(Firstbuf, ui.current_buffer));
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    setCurrentBuffer(ui.current_buffer->nextBuffer);
}

static int
checkBackBuffer(struct Buffer* buf)
{
    if (buf->nextBuffer)
        return true;

    return false;
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK, "Close current buffer and return to the one below in stack")
{
    if (!ui.current_buffer->nextBuffer) {
        message(getUI(), MSG_INFO, "Can't go back...");
        return;
    }
    delBuffer(ui.current_buffer);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    struct Buffer* buf = ui.current_buffer->nextBuffer;
    if (buf)
        delBuffer(buf);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    goURL0(ui, "Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    const char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        struct Url p_url;
        struct Buffer* cur_buf = ui.current_buffer;
        SKIP_BLANKS(url);
        url = url_quote(url);
        p_url = parseUrl(url, NULL);
        pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
        struct Content c = getContent(url, NULL, NULL, NULL, UI_TTY);
        pushContent(c, ui.viewport.size.x, ui.use_graphic);
        if (ui.current_buffer != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&ui.current_buffer->content.url)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0(ui, "Goto relative URL: ", true);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    struct Content c = getContent(BookmarkFile, NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
}

#define W3MBOOKMARK_CMDNAME "w3mbookmark"
// #define W3MBOOKMARK_CMDNAME "w3mbookmark.exe"

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    Str tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                      "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&ui.current_buffer->content.url)))->ptr,
        (Str_form_quote(wc_conv_strict(ui.current_buffer->document.title,
             InnerCharset,
             BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(BookmarkCharset));
    struct Form* post = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    post->body = tmp->ptr;
    post->length = tmp->length;
    struct Content c = getContent("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, post, NO_REFERER, UI_TTY);
    pushContent(c, ui.viewport.size.x, ui.use_graphic);
}

