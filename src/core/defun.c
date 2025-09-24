#include "AnchorList.h"
#include "w3m.h"
#include "cookie.h"
#include "HtmlTagParsed.h"
#include "document_renderer.h"
#include "menu.h"
#include "LinkList.h"
#include "internal.h"
#include "page_info.h"
#include "Anchor.h"
#include "HttpRequest.h"
#include "follow_anchor.h"
#include "myctype.h"
#include "regex.h"
#include "form.h"
#include "http_message.h"
#include "mailcap.h"
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
#include "rc.h"
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
    ui->current_buffer->document.topLineIndex += ui->viewport.size.y;
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
    ui->current_buffer->document.topLineIndex++;
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    ui->current_buffer->document.topLineIndex--;
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    int offsety = ui->viewport.size.y / 2 - ui->viewport_cursor.y;
    if (offsety) {
        ui->current_buffer->document.topLineIndex = ui->current_buffer->document.topLineIndex - offsety;
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    int offsetx = ui->viewport_cursor.x - ui->viewport.size.x / 2;
    if (offsetx) {
        ui->current_buffer->document.currentColumn += offsetx;
    }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    // ui->current_buffer->document = 0;
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
    int column = ui->current_buffer->document.currentColumn;
    ui->current_buffer->document.currentColumn += (ui->searchkey_num * (-ui->viewport.size.x + 1) + 1);
    shiftvisualpos(ui, ui->current_buffer, ui->current_buffer->document.currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    int column = ui->current_buffer->document.currentColumn;
    ui->current_buffer->document.currentColumn += (ui->searchkey_num * (ui->viewport.size.x - 1) - 1);
    shiftvisualpos(ui, ui->current_buffer, ui->current_buffer->document.currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct LineList* l = currentLine(&ui->current_buffer->document);
    if (l == NULL)
        return;

    int n = ui->searchkey_num;
    for (int j = 0; j < n; j++) {
        int column = ui->current_buffer->document.currentColumn;
        ui->current_buffer->document.currentColumn += 1;
        if (column == ui->current_buffer->document.currentColumn)
            break;
        shiftvisualpos(ui, ui->current_buffer, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    struct LineList* l = currentLine(&ui->current_buffer->document);
    if (l == NULL)
        return;
    int n = ui->searchkey_num;
    for (int j = 0; j < n; j++) {
        if (ui->current_buffer->document.currentColumn == 0)
            break;
        ui->current_buffer->document.currentColumn += (-1);
        shiftvisualpos(ui, ui->current_buffer, -1);
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
    struct Content c = getContent(ui, file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER);
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    const char* lang = AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    struct Content c = getContent(ui, tmp->ptr, NULL, NULL, NO_REFERER);
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
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
next_nonnull_line(struct UI *ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->next)
        ;

    if (l == NULL || l->l.len == 0)
        return -1;

    ui->current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui->current_buffer->document.pos = 0;
    return 0;
}

static wc_uint32
getChar(const char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

DEFUN(movRW, NEXT_WORD, "Move to the next word")
{
    int n = ui->searchkey_num;
    for (int i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui->current_buffer->document);
        int ppos = ui->current_buffer->document.pos;

        if (next_nonnull_line(ui, currentLine(&ui->current_buffer->document)) < 0)
            return;

        struct LineList* l = currentLine(&ui->current_buffer->document);
        const char* lb = l->l.lineBuf;
        while (ui->current_buffer->document.pos < l->l.len && wc_is_ucs_alnum(getChar(&lb[ui->current_buffer->document.pos])))
            ui->current_buffer->document.pos = nextChar(ui->current_buffer->document.pos, &l->l);

        while (1) {
            while (ui->current_buffer->document.pos < l->l.len && !wc_is_ucs_alnum(getChar(&lb[ui->current_buffer->document.pos])))
                ui->current_buffer->document.pos = nextChar(ui->current_buffer->document.pos, &l->l);
            if (ui->current_buffer->document.pos < l->l.len)
                break;
            if (next_nonnull_line(ui, currentLine(&ui->current_buffer->document)->next) < 0) {
                ui->current_buffer->document.currentLineIndex = pline->linenumber;
                ui->current_buffer->document.pos = ppos;
                return;
            }
            ui->current_buffer->document.pos = 0;
            l = currentLine(&ui->current_buffer->document);
            lb = l->l.lineBuf;
        }
    }
}

static int
prev_nonnull_line(struct UI *ui, struct LineList* line)
{
    struct LineList* l;
    for (l = line; l != NULL && l->l.len == 0; l = l->prev)
        ;
    if (l == NULL || l->l.len == 0)
        return -1;

    ui->current_buffer->document.currentLineIndex = l->linenumber;
    if (l != line)
        ui->current_buffer->document.pos = currentLine(&ui->current_buffer->document)->l.len;
    return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    int n = ui->searchkey_num;
    for (int i = 0; i < n; i++) {
        struct LineList* pline = currentLine(&ui->current_buffer->document);
        int ppos = ui->current_buffer->document.pos;

        if (prev_nonnull_line(ui, currentLine(&ui->current_buffer->document)) < 0)
            goto end;

        while (1) {
            struct LineList* l = currentLine(&ui->current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui->current_buffer->document.pos > 0) {
                int tmp = prevChar(ui->current_buffer->document.pos, &l->l);
                if (wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui->current_buffer->document.pos = tmp;
            }
            if (ui->current_buffer->document.pos > 0)
                break;
            if (prev_nonnull_line(ui, currentLine(&ui->current_buffer->document)->prev) < 0) {
                ui->current_buffer->document.currentLineIndex = pline->linenumber;
                ui->current_buffer->document.pos = ppos;
                goto end;
            }
            ui->current_buffer->document.pos = currentLine(&ui->current_buffer->document)->l.len;
        }

        {
            struct LineList* l = currentLine(&ui->current_buffer->document);
            const char* lb = l->l.lineBuf;
            while (ui->current_buffer->document.pos > 0) {
                int tmp = prevChar(ui->current_buffer->document.pos, &l->l);
                if (!wc_is_ucs_alnum(getChar(&lb[tmp])))
                    break;
                ui->current_buffer->document.pos = tmp;
            }
        }
    }
end:
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit without confirmation")
{
    // _quitfm(false);
}

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request")
{
    // _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel")
{
    struct Buffer* buf;
    int ok;
    char cmd;

    ok = false;
    do {
        buf = selectBuffer(ui, getFirstbuf(), ui->current_buffer, &cmd);
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
            qquitfm(ui);
            break;
        case 'Q':
            quitfm(ui);
            break;
        }
    } while (!ok);

    for (buf = getFirstbuf(); buf != NULL; buf = buf->nextBuffer) {
        if (buf == ui->current_buffer)
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
        _goLine(ui, inputStr(ui, "Goto line: ", ""));
}

DEFUN(goLineL, END, "Go to the last line")
{
    _goLine(ui, "$");
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line")
{
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    while (currentLine(&ui->current_buffer->document)->next
        && currentLine(&ui->current_buffer->document)->next->bpos)
        cursorDown(1);
    ui->current_buffer->document.pos = currentLine(&ui->current_buffer->document)->l.len - 1;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source")
{
    // const char* fn = ui->current_buffer->filename;
    // if (fn == NULL
    //     || (ui->current_buffer->content.cc.content_type == CONTENTTYPE_UNKNOWN && ui->current_buffer->edit == NULL)
    //     || /* Reading shell */ ui->current_buffer->content.url.scheme != SCM_LOCAL
    //     || !strcmp(ui->current_buffer->content.url.file, "-") /* file is std input  */
    // ) {
    //     message(ui, MSG_ERR, "Can't edit other than local file");
    //     return;
    // }

    Str cmd;
    if (ui->current_buffer->edit)
        cmd = unquote_mailcap(ui->current_buffer->edit,
            contentTypeStr(ui->current_buffer->content.cc.content_type), ui->current_buffer->content.sourcefile,
            getHttpHeaderValue(ui->current_buffer->content.document_header, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(ui->current_buffer->content.sourcefile), 1);
    exec_cmd(cmd->ptr);
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document")
{
    const char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        /* FIXME: gettextize? */
        message(ui, MSG_ERR, Sprintf("Can't open %s", tmpf)->ptr);
        return;
    }

    saveBuffer(ui->current_buffer, f, true);
    fclose(f);
    exec_cmd(myEditor(Editor, shell_quote(tmpf),
        1
        // cur_real_linenumber(ui->current_buffer)
        )
            ->ptr);
    unlink(tmpf);
}

/* Set / unset mark */
DEFUN(_mark, MARK, "Set/unset mark")
{
    if (!use_mark)
        return;
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    struct LineList* l = currentLine(&ui->current_buffer->document);
    l->l.propBuf[ui->current_buffer->document.pos] ^= PE_MARK;
}

/* Go to next mark */
DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    if (!use_mark)
        return;
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    int i = ui->current_buffer->document.pos + 1;
    struct LineList* l = currentLine(&ui->current_buffer->document);
    if (i >= l->l.len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->l.len; i++) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui->current_buffer->document.currentLineIndex = l->linenumber;
                ui->current_buffer->document.pos = i;

                return;
            }
        }
        l = l->next;
        i = 0;
    }
    /* FIXME: gettextize? */
    message(ui, MSG_INFO, "No mark exist after here");
}

/* Go to previous mark */
DEFUN(prevMk, PREV_MARK, "Go to the previous mark")
{
    if (!use_mark)
        return;
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    int i = ui->current_buffer->document.pos - 1;
    struct LineList* l = currentLine(&ui->current_buffer->document);
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->l.propBuf[i] & PE_MARK) {
                ui->current_buffer->document.currentLineIndex = l->linenumber;
                ui->current_buffer->document.pos = i;

                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->l.len - 1;
    }
    /* FIXME: gettextize? */
    message(ui, MSG_INFO, "No mark exist before here");
}

static const char* MarkString = NULL;

/* Mark place to which the regular expression matches */
DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    if (!use_mark)
        return;

    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist(ui, "(Mark)Regexp: ", MarkString, TextHist);
        if (str == NULL || *str == '\0') {

            return;
        }
    }
    str = conv_search_string(ui, str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
        message(ui, MSG_INFO, str);
        return;
    }

    struct LineList* l;
    MarkString = str;
    for (l = ui->current_buffer->document.firstLine; l != NULL; l = l->next) {
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
    struct HmarkerList* hl = ui->current_buffer->document.hmarklist;
    if (ui->current_buffer->document.firstLine == NULL)
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
        an = retrieveAnchor(ui->current_buffer->document.href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui->current_buffer->document.formitem, *po);
        hseq++;
    } while (an == NULL);

    gotoLine(&ui->current_buffer->document, po->line);
    ui->current_buffer->document.pos = po->pos;
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink")
{
    struct HmarkerList* hl = ui->current_buffer->document.hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq;

    if (ui->current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(ui->current_buffer->document.href, *po);
        if (an == NULL)
            an = retrieveAnchor(ui->current_buffer->document.formitem, *po);
        hseq--;
    } while (an == NULL);

    gotoLine(&ui->current_buffer->document, po->line);
    ui->current_buffer->document.pos = po->pos;
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link")
{
    struct HmarkerList* hl = ui->current_buffer->document.hmarklist;

    int n = ui->searchkey_num;
    if (n < 0 || n > hl->nmark)
        return;

    if (ui->current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    struct BufferPoint* po = hl->marks + n - 1;
    struct Anchor* an = retrieveAnchor(ui->current_buffer->document.href, *po);
    if (an == NULL)
        an = retrieveAnchor(ui->current_buffer->document.formitem, *po);
    if (an == NULL)
        return;

    gotoLine(&ui->current_buffer->document, po->line);
    ui->current_buffer->document.pos = po->pos;
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
nextX(struct UI *ui, int d, int dy)
{
    if (ui->current_buffer->document.firstLine == NULL)
        return;

    struct HmarkerList* hl = ui->current_buffer->document.hmarklist;
    if (!hl || hl->nmark == 0)
        return;

    struct Anchor* an = retrieveAnchor(ui->current_buffer->document.href, getBufferPosition(ui->current_buffer));
    if (an == NULL)
        an = retrieveAnchor(ui->current_buffer->document.formitem, getBufferPosition(ui->current_buffer));

    int y;
    struct Anchor* pan = getNextHorizontalAnchor(&ui->current_buffer->document, an, ui->searchkey_num, d, dy);
    if (pan == NULL)
        return;

    ui->current_buffer->document.pos = pan->start.pos;
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
nextY(struct UI *ui, int d)
{
    struct HmarkerList* hl = ui->current_buffer->document.hmarklist;
    struct Anchor* pan;
    int i, x, y, n = ui->searchkey_num;
    int hseq;

    if (ui->current_buffer->document.firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    struct Anchor* an = retrieveAnchor(ui->current_buffer->document.href, getBufferPosition(ui->current_buffer));
    if (an == NULL)
        an = retrieveAnchor(ui->current_buffer->document.formitem, getBufferPosition(ui->current_buffer));

    x = ui->current_buffer->document.pos;
    y = currentLine(&ui->current_buffer->document)->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++) {
        if (an)
            hseq = abs(an->hseq);
        an = NULL;
        for (; y >= 0 && y <= lastLine(&ui->current_buffer->document)->linenumber; y += d) {
            struct BufferPoint bp = { .line = y, .pos = x };
            an = retrieveAnchor(ui->current_buffer->document.href, bp);
            if (!an)
                an = retrieveAnchor(ui->current_buffer->document.formitem, bp);
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
    gotoLine(&ui->current_buffer->document, pan->start.line);
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
    setCurrentBuffer(prevBuffer(getFirstbuf(), ui->current_buffer));
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    setCurrentBuffer(ui->current_buffer->nextBuffer);
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
    if (!ui->current_buffer->nextBuffer) {
        message(ui, MSG_INFO, "Can't go back...");
        return;
    }
    delBuffer(ui->current_buffer);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    struct Buffer* buf = ui->current_buffer->nextBuffer;
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
        struct Buffer* cur_buf = ui->current_buffer;
        SKIP_BLANKS(url);
        url = url_quote(url);
        p_url = parseUrl(url, NULL);
        pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
        struct Content c = getContent(ui, url, NULL, NULL, NULL);
        pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
        if (ui->current_buffer != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&ui->current_buffer->content.url)->ptr);
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    goURL0(ui, "Goto relative URL: ", true);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    struct Content c = getContent(ui, BookmarkFile, NULL, NULL, NO_REFERER);
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
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
        (Str_form_quote(parsedURL2Str(&ui->current_buffer->content.url)))->ptr,
        (Str_form_quote(wc_conv_strict(ui->current_buffer->document.title,
             InnerCharset,
             BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(BookmarkCharset));
    struct Form* post = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    post->body = tmp->ptr;
    post->length = tmp->length;
    struct Content c = getContent(ui, "file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, post, NO_REFERER);
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    struct Content c = makeContentFromHtmlUtf8(load_option_panel_html());
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */

    const char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            const char* v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist(ui, "Set option: ", opt, TextHist);
        if (opt == NULL || *opt == '\0') {

            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option(ui);
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages")
{
    struct Content c = makeContentFromHtmlUtf8(message_list_panel_html());
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document")
{
    struct Content c = makeContentFromHtmlUtf8(page_info_panel_html(
        &ui->current_buffer->content,
        &ui->current_buffer->document,
        getBufferPosition(ui->current_buffer)));
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    struct LinkList* l = link_menu(ui, &ui->current_buffer->document);
    if (!l || !l->url)
        return;

    if (*(l->url) == '#') {
        gotoLabel(ui, l->url + 1);
        return;
    }

    struct Url p_url = parseUrl(l->url, makeBaseUrl(&ui->current_buffer->document));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content c = getContent(ui, l->url, makeBaseUrl(&ui->current_buffer->document),
        NULL, parsedURL2Str(&ui->current_buffer->content.url)->ptr);
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* accesskey */
DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu")
{
    anchorMn(ui, accesskey_menu, true);
}

/* list menu */
DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to")
{
    anchorMn(ui, list_menu, true);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks")
{
    anchorMn(ui, list_menu, false);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced")
{
    struct Content c = makeContentFromHtmlUtf8(link_list_panel_html(&ui->current_buffer->document));
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list")
{
    struct Content c = makeContentFromHtmlUtf8(cookie_list_panel_html());
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history")
{
    struct Content c = makeContentFromHtmlUtf8(historyBuffer_html(URLHist));
    pushContent(ui, c, ui->viewport.size.x, ui->use_graphic);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followAnchor(ui, true);
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    followImage(ui, true);
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* file = searchKeyData();
    const char* qfile = NULL;
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist(ui, "Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0') {

            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);

    bool is_pipe;
    FILE* f;
    if (*file == '|') {
        is_pipe = true;
        f = popen(file + 1, "w");
    } else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (!notExistsOrOverWrite(ui, file)) {
            return;
        }
        f = fopen(file, "w");
        is_pipe = false;
    }
    if (f == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        message(ui, MSG_ERR, emsg);
        return;
    }
    saveBuffer(ui->current_buffer, f, true);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (ui->current_buffer->content.sourcefile == NULL)
        return;
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    PermitSaveToPipe = true;
    // if (ui->current_buffer->real_scheme == SCM_LOCAL)
    //     file = conv_from_system(guessSaveName(NULL, ui->current_buffer->content.url.real_file));
    // else
    const char* file = guessSaveName(ui->current_buffer->content.document_header, ui->current_buffer->content.url.file);
    doFileCopy(ui->current_buffer->content.sourcefile, file);
    PermitSaveToPipe = false;
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address")
{
    // _peekURL(ui, 0);
}

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address")
{
    // _peekURL(ui, 1);
}

DEFUN(curURL, PEEK, "Show current address")
{
}

/* view HTML source */
DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    if (ui->current_buffer->content.cc.content_type == CONTENTTYPE_UNKNOWN) {
        return;
    }
    if (ui->current_buffer->content.sourcefile == NULL) {
        return;
    }

    struct Buffer* buf = newBuffer();

    if (ui->current_buffer->content.cc.content_type == CONTENTTYPE_TEXT_HTML) {
        buf->content.cc.content_type = CONTENTTYPE_TEXT_PLAIN;
        buf->document.title = Sprintf("source of %s", ui->current_buffer->document.title)->ptr;
    } else if (ui->current_buffer->content.cc.content_type == CONTENTTYPE_TEXT_PLAIN) {
        buf->content.cc.content_type = CONTENTTYPE_TEXT_HTML;
        buf->document.title = Sprintf("HTML view of %s", ui->current_buffer->document.title)->ptr;
    } else {
        return;
    }
    buf->content.url = ui->current_buffer->content.url;
    buf->content.sourcefile = ui->current_buffer->content.sourcefile;
    buf->document.charset = ui->current_buffer->document.charset;
    buf->clone = ui->current_buffer->clone;
    (*buf->clone)++;

    pushBuffer(buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew")
{
    // if (ui->current_buffer->bufferprop & BP_INTERNAL) {
    //     if (!strcmp(ui->current_buffer->document.title, DOWNLOAD_LIST_TITLE)) {
    //         ldDL(ui);
    //         return;
    //     }
    //     /* FIXME: gettextize? */
    //     message(ui, MSG_ERR, "Can't reload...");
    //     return;
    // }
    if (ui->current_buffer->content.url.scheme == SCM_LOCAL && !strcmp(ui->current_buffer->content.url.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        message(ui, MSG_ERR, "Can't reload stdin");
        return;
    }

    int multipart = 0;

    struct Form* post;
    if (ui->current_buffer->form_submit) {
        post = ui->current_buffer->form_submit->parent;
        if (post->method == FORM_METHOD_POST
            && post->enctype == FORM_ENCTYPE_MULTIPART) {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(&ui->current_buffer->document, getBufferPosition(ui->current_buffer),
                &query, ui->current_buffer->form_submit, multipart);
            stat(post->body, &st);
            post->length = st.st_size;
        }
    } else {
        post = NULL;
    }
    Str url = parsedURL2Str(&ui->current_buffer->content.url);
    message(ui, MSG_INFO, "Reloading...");
    // refresh(ttyWriter());
    wc_ces old_charset = DocumentCharset;
    if (ui->current_buffer->document.charset != WC_CES_US_ASCII)
        DocumentCharset = ui->current_buffer->document.charset;
    // SearchHeader = ui->current_buffer->search_header;
    DefaultType = contentTypeStr(ui->current_buffer->content.cc.content_type);
    struct Content c = getContent(ui, url->ptr, NULL, post, NO_REFERER /*, true*/);

    struct Buffer* buf = makeBuffer(&c, ui->viewport.size.x, ui->use_graphic);
    DocumentCharset = old_charset;
    // SearchHeader = false;
    DefaultType = NULL;

    if (multipart)
        unlink(post->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        message(ui, MSG_ERR, "Can't reload...");
        return;
    } else if (buf) {

        return;
    }

    // struct Buffer *fbuf = NULL;
    // if (fbuf != NULL)
    //     Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(ui->current_buffer, buf);
    // if ((buf->content.cc.content_type == CONTENTTYPE_TEXT_PLAIN && sbuf.content.cc.content_type == CONTENTTYPE_TEXT_HTML)
    //     || (buf->content.cc.content_type == CONTENTTYPE_TEXT_HTML && sbuf.content.cc.content_type == CONTENTTYPE_TEXT_PLAIN)) {
    //     vwSrc(ui);
    //     if (ui->current_buffer != buf)
    //         Firstbuf = deleteBuffer(Firstbuf, buf);
    // }
    // ui->current_buffer->form_submit = sbuf.form_submit;
    if (ui->current_buffer->document.firstLine) {
        // COPY_BUFROOT(ui->current_buffer, &sbuf);
        // restorePosition(ui->current_buffer, &sbuf);
    }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    ui->current_buffer->document.cols = 0;
}

DEFUN(docCSet, CHARSET, "Change the character encoding for the current document")
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(ui, "Document charset: ",
            wc_ces_to_charset(ui->current_buffer->document.charset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        return;
    }
    // _docCSet(ui, charset);
    if (ui->current_buffer->content.sourcefile == NULL) {
        message(ui, MSG_INFO, "Can't reload...");
        return;
    }
    ui->current_buffer->document.charset = charset;
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr(ui, "Default document charset: ",
            wc_ces_to_charset(DocumentCharset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks")
{
    chkURLBuffer(ui->current_buffer);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink")
{
    int spos, epos;
    const char* p = getCurWord(ui->current_buffer, &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(ui->current_buffer, currentLine(&ui->current_buffer->document), spos, epos);
}

/* show current line number and number of lines in the entire document */
// DEFUN(curlno, LINE_INFO, "Display current position in document")
// {
//     struct Line* l = currentLine(&ui->current_buffer->document);
//     Str tmp;
//     int cur = 0, all = 0, col = 0, len = 0;
//
//     if (l != NULL) {
//         cur = l->real_linenumber;
//         col = l->bwidth + ui->current_buffer->currentColumn + ui->current_buffer->cursorX + 1;
//         while (l->next && l->next->bpos)
//             l = l->next;
//         if (l->width < 0)
//             l->width = COLPOS(l, l->len);
//         len = l->bwidth + l->width;
//     }
//     if (lastLine(&ui->current_buffer->document))
//         all = lastLine(&ui->current_buffer->document)->real_linenumber;
//     tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
//         (int)((double)cur * 100.0 / (double)(all ? all : 1)
//             + 0.5),
//         col, len);
//     Strcat_charp(tmp, "  ");
//     Strcat_charp(tmp, wc_ces_to_charset_desc(ui->current_buffer->document_charset));
//
//     message(ui, MSG_INFO, tmp->ptr);
// }

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = true;
    /*
     * if (!(ui->current_buffer->type && is_html_type(ui->current_buffer->type)))
     * return;
     */
    ui->current_buffer->document.image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!activeImage)
        return;
    /*
     * if (!(ui->current_buffer->type && is_html_type(ui->current_buffer->type)))
     * return;
     */
    ui->current_buffer->document.image_flag = IMG_FLAG_SKIP;
}

DEFUN(dispVer, VERSION, "Display the version of w3m")
{
    message(ui, MSG_INFO, Sprintf("w3m version %s", w3m_version)->ptr);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches")
{
    if (WrapSearch) {
        WrapSearch = false;
        /* FIXME: gettextize? */
        message(ui, MSG_INFO, "Wrap search off");
    } else {
        WrapSearch = true;
        /* FIXME: gettextize? */
        message(ui, MSG_INFO, "Wrap search on");
    }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(ui, inputStr(ui, "(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(ui, GetWord(ui->current_buffer));
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(ui, "command [; ...]: ", "", TextHist);
        if (data == NULL) {

            return;
        }
    }
    /* data: FUNC [DATA] [; FUNC [DATA] ...] */
    while (*data) {
        SKIP_BLANKS(data);
        if (*data == ';') {
            data++;
            continue;
        }
        const char* p = getWord(&data);
        CommandFunc func = getFunc(p);
        p = getQWord(&data);
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = *p ? p : NULL;
        func(ui);
        CurrentCmdData = NULL;
    }
}

DEFUN(setAlarm, ALARM, "Set alarm")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(ui, "(Alarm)sec command: ", "", TextHist);
        if (data == NULL) {

            return;
        }
    }
    CommandFunc cmd = NULL;
    int sec = 0;
    if (*data) {
        sec = atoi(getWord(&data));
        if (sec > 0)
            cmd = getFunc(getWord(&data));
    }
    // if (cmd >= 0)
    {
        data = getQWord(&data);
        // TODO:
        // setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, data);
        // message(ui, MSG_INFO, Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr);
    }
    // else {
    //     setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, FUNCNAME_nulcmd, NULL);
    // }
}

DEFUN(reinit, REINIT, "Reload configuration file")
{
    const char* resource = searchKeyData();
    if (resource == NULL) {
        init_rc();
        sync_with_option(ui);
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option(ui);

        return;
    }

    if (!strcasecmp(resource, "COOKIE")) {
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "KEYMAP")) {
        initKeymap(ui, true);
        return;
    }

    if (!strcasecmp(resource, "MAILCAP")) {
        initMailcap();
        return;
    }

    if (!strcasecmp(resource, "MENU")) {
        initMenu();
        return;
    }

    if (!strcasecmp(resource, "MIMETYPES")) {
        // initMimeTypes();
        return;
    }

    message(ui, MSG_ERR, Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist(ui, "Key definition: ", "", TextHist);
        if (data == NULL || *data == '\0') {

            return;
        }
    }
    setKeymap(ui, allocStr(data, -1), -1);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel")
{
    // int replace = false;
    // // if (ui->current_buffer->bufferprop & BP_INTERNAL && !strcmp(ui->current_buffer->document.title, DOWNLOAD_LIST_TITLE))
    // //     replace = true;
    // if (!FirstDL) {
    //     if (replace) {
    //         if (ui->current_buffer == Firstbuf && ui->current_buffer->nextBuffer == NULL) {
    //         } else
    //             delBuffer(ui->current_buffer);
    //     }
    //     return;
    // }
    // int reload = checkDownloadList();
    //
    // struct Content c = makeContentFromHtmlUtf8(DownloadListBuffer_html());
    // if (!c.page) {
    //     return;
    // }
    // // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    // if (replace) {
    //     // COPY_BUFROOT(buf, ui->current_buffer);
    //     // restorePosition(buf, ui->current_buffer);
    // }
    // pushContent(c, ui->viewport.size.x, ui->use_graphic);
    // if (replace)
    //     deletePrevBuf(ui);
    // if (reload)
    //     ui->current_buffer->event = setAlarmEvent(ui->current_buffer->event, 1, AL_IMPLICIT,
    //         FUNCNAME_reload, NULL);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    if (!ui->current_buffer->document.firstLine)
        return;

    struct BufferPos* b = ui->current_buffer->undo;
    if (!b || !b->prev)
        return;

    resetPos(ui->current_buffer, b);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    if (!ui->current_buffer->document.firstLine)
        return;

    struct BufferPos* b = ui->current_buffer->undo;
    if (!b || !b->next)
        return;

    resetPos(ui->current_buffer, b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    ui->current_buffer->document.currentLineIndex = 0;
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    int offsety = (ui->viewport.size.y - 1) / 2;
    ui->current_buffer->document.currentLineIndex += offsety;
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (ui->current_buffer->document.firstLine == NULL)
        return;
    int offsety = ui->viewport.size.y - 1;
    ui->current_buffer->document.currentLineIndex += offsety;
}

DEFUN(goLineF, BEGIN, "Go to the first line")
{
    _goLine(ui, "^");
}

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line")
{
    ui->current_buffer->document.pos = 0;
}
