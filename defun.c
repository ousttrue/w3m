#include "defun.h"
#include "option.h"
#include "dict.h"
#include "cookie.h"
#include "menu.h"
#include "file.h"
#include "history.h"
#include "tab_list.h"
#include "hmarker.h"
#include "html_form.h"
#include "regex.h"
#include "funcheader.h"
#include "etc.h"
#include "mailcap.h"
#include "indep.h"
#include "message.h"
#include "mysignal.h"
#include "local_cgi.h"
#include "linein.h"
#include "w3m_rc.h"
#include "myctype.h"
#include "func.h"
#include "tab.h"
#include "buffer.h"
#include "document.h"
#include "screen.h"
#include "search.h"
#include <libwc/charset.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{
}

DEFUN(quitfm, ABORT EXIT, "Quit without confirmation")
{
    _quitfm(false);
}

DEFUN(qquitfm, QUIT, "Quit with confirmation request")
{
    _quitfm(getRuntime()->confirm_on_quit);
}

DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background")
{
    screen_move((struct Vec2) { .y = LASTLINE(), .x = 0 });
    screen_clrtoeolx();
    tty_write_screen();
    exitRawMode();
    const char* shell = getenv("SHELL");
    if (!shell) {
        shell = "/bin/sh";
    }
    system(shell);
    enterRawMode();
}

DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    tty_clear();
    screen_clear();
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(setEnv, SETENV, "Set environment variable")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, getRuntime()->TextHist);
        if (env == NULL || *env == '\0') {
            return;
        }
    }

    char* value = strchr(env, '=');
    if (value && value > env) {
        char* var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
}

DEFUN(editBf, EDIT, "Edit local source")
{
    const char* fn = ctx.buf->content->filename;
    // if (fn == NULL || ctx.buf->pagerSource != NULL || /* Behaving as a pager */
    //     (ctx.buf->type == NULL && ctx.buf->edit == NULL) || /* Reading shell */
    //     ctx.buf->real_scheme != SCM_LOCAL || !strcmp(ctx.buf->currentURL.file, "-") || /* file is std input  */
    //     ctx.buf->bufferprop & BP_FRAME) { /* Frame */
    //     disp_err_message("Can't edit other than local file", TRUE);
    //     return;
    // }

    Str cmd;
    if (ctx.buf->edit)
        cmd = unquote_mailcap(ctx.buf->edit, ctx.buf->content->content_type, fn,
            checkHeader(ctx.buf->content, "Content-Type:"), NULL);
    else
        cmd = myEditor(getRuntime()->Editor, shell_quote(fn), doc_cur_real_linenumber(ctx.buf->doc));
    blockChild(cmd->ptr);

    // buffer is modified. so reload
    reload(ctx);
}

DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document")
{
    const char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (!f) {
        disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, TRUE);
        return;
    }
    saveBuffer(ctx.buf, f, TRUE);
    fclose(f);
    exec_cmd(myEditor(getRuntime()->Editor, shell_quote(tmpf), doc_cur_real_linenumber(ctx.buf->doc))->ptr);
    unlink(tmpf);
}

DEFUN(escmap, ESCMAP, "ESC map")
{
    // int c = getch();
    // if (IS_ASCII(c))
    //     escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map")
{
    // int c = getch();
    // if (IS_DIGIT(c)) {
    //     return;
    // }
    // if (IS_ASCII(c))
    //     escKeyProc((int)c, K_ESCB, EscBKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap")
{
    // char c = getch();
    // if (IS_ASCII(c)) {
    //     getRuntime()->CurrentKey = K_MULTI | (getRuntime()->CurrentKey << 16) | c;
    //     escKeyProc((int)c, 0, NULL);
    // }
}

//
// shell
//
DEFUN(pipeBuf, PIPE_BUF, "Pipe current buffer through a shell command and display output")
{
    // getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    // char* cmd = searchKeyData();
    // if (cmd == NULL || *cmd == '\0') {
    //     /* FIXME: gettextize? */
    //     cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, getRuntime()->ShellHist);
    // }
    // if (cmd != NULL)
    //     cmd = conv_to_system(cmd);
    // if (cmd == NULL || *cmd == '\0') {
    //     return;
    // }
    //
    // char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    // FILE* f = fopen(tmpf, "w");
    // if (f == NULL)
    //     if (getRuntime()->UseHistory)
    //         loadHistory(getRuntime()->URLHist);
    //
    // if (getRuntime()->UseHistory)
    //     loadHistory(getRuntime()->URLHist);
    //
    // {
    //     /* FIXME: gettextize? */
    //     disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
    //     return;
    // }
    // saveBuffer(ctx.buf, f, TRUE);
    // fclose(f);
    // struct Buffer* buf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
    // if (buf == NULL) {
    //     disp_message("Execution failed", TRUE);
    //     return;
    // } else {
    //     buf->content.filename = cmd;
    //     buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
    //         conv_from_system(cmd))
    //                           ->ptr;
    //     buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    //     if (buf->type == NULL)
    //         buf->type = "text/plain";
    //     buf->currentURL.file = "-";
    //     pushBuffer(buf);
    // }
}

/* Execute shell command and read output ac pipe. */
DEFUN(pipesh, PIPE_SHELL, "Execute shell command and display output")
{
    // getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    // char* cmd = searchKeyData();
    // if (cmd == NULL || *cmd == '\0') {
    //     cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, getRuntime()->ShellHist);
    // }
    // if (cmd != NULL)
    //     cmd = conv_to_system(cmd);
    // if (cmd == NULL || *cmd == '\0') {
    //     return;
    // }
    //
    // struct Buffer* buf = getpipe(cmd);
    // if (buf == NULL) {
    //     disp_message("Execution failed", TRUE);
    //     return;
    // } else {
    //     buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    //     if (buf->type == NULL)
    //         buf->type = "text/plain";
    //     pushBuffer(buf);
    // }
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, getRuntime()->ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
        return;
    }
    auto prevtrap = mySignal(SIGINT, intTrap);
    exitRawMode();
    struct Buffer* buf = getshell(cmd);
    mySignal(SIGINT, prevtrap);
    enterRawMode();
    if (buf == NULL) {
        disp_message("Execution failed", TRUE);
        return;
    } else {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->content->content_type == NULL)
            buf->content->content_type = "text/plain";
        tab_push_buffer(CurrentTab(), buf);
    }
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, getRuntime()->ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0') {
        exitRawMode();
        printf("\n");
        (void)!system(cmd); /* We do not care about the exit code here! */
        /* FIXME: gettextize? */
        printf("\n[Hit any key]");
        fflush(stdout);
        enterRawMode();
        getch();
    }
}

//
// cursor, scroll
//
DEFUN(movL, MOVE_LEFT, "Cursor left")
{
    doc_movL(ctx.buf->doc, ctx.buf->doc->COLS / 2);
}

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide")
{
    doc_movL(ctx.buf->doc, 1);
}

DEFUN(movD, MOVE_DOWN, "Cursor down")
{
    doc_movD(ctx.buf->doc, (ctx.buf->doc->LINES + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide")
{
    doc_movD(ctx.buf->doc, 1);
}

DEFUN(movU, MOVE_UP, "Cursor up")
{
    doc_movU(ctx.buf->doc, (ctx.buf->doc->LINES + 1) / 2);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide")
{
    doc_movU(ctx.buf->doc, 1);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right")
{
    doc_movR(ctx.buf->doc, ctx.buf->doc->COLS / 2);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide")
{
    doc_movR(ctx.buf->doc, 1);
}

DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;
    while (ctx.buf->doc->currentLine->prev && ctx.buf->doc->currentLine->bpos)
        doc_cursorUp0(ctx.buf->doc, 1);
    ctx.buf->doc->pos = 0;
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(linend, LINE_END, "Go to the end of the line")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;
    while (ctx.buf->doc->currentLine->next
        && ctx.buf->doc->currentLine->next->bpos)
        doc_cursorDown0(ctx.buf->doc, 1);
    ctx.buf->doc->pos = ctx.buf->doc->currentLine->len - 1;
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line")
{
    const char* str = searchKeyData();
    if (getRuntime()->prec_num)
        doc_goLine(ctx.buf->doc, "^");
    else if (str)
        doc_goLine(ctx.buf->doc, str);
    else
        doc_goLine(ctx.buf->doc, inputStr("Goto line: ", ""));
}

DEFUN(goLineF, BEGIN, "Go to the first line")
{
    doc_goLine(ctx.buf->doc, "^");
}

DEFUN(goLineL, END, "Go to the last line")
{
    doc_goLine(ctx.buf->doc, "$");
}

DEFUN(movLW, PREV_WORD, "Move to the previous word")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;

    // char* lb;
    // struct Line *pline, *l;
    // int ppos;

    int n = searchKeyNum();
    for (int i = 0; i < n; i++) {
        struct Line* pline = ctx.buf->doc->currentLine;
        int ppos = ctx.buf->doc->pos;

        if (!doc_prev_nonnull_line(ctx.buf->doc, ctx.buf->doc->currentLine))
            goto end;

        while (1) {
            struct Line* l = ctx.buf->doc->currentLine;
            const char* lb = l->lineBuf;
            while (ctx.buf->doc->pos > 0) {
                int tmp = ctx.buf->doc->pos;
                prevChar(tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                ctx.buf->doc->pos = tmp;
            }
            if (ctx.buf->doc->pos > 0)
                break;
            if (!doc_prev_nonnull_line(ctx.buf->doc, ctx.buf->doc->currentLine->prev)) {
                ctx.buf->doc->currentLine = pline;
                ctx.buf->doc->pos = ppos;
                goto end;
            }
            ctx.buf->doc->pos = ctx.buf->doc->currentLine->len;
        }

        struct Line* l = ctx.buf->doc->currentLine;
        const char* lb = l->lineBuf;
        while (ctx.buf->doc->pos > 0) {
            int tmp = ctx.buf->doc->pos;
            prevChar(tmp, l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            ctx.buf->doc->pos = tmp;
        }
    }
end:
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(movRW, NEXT_WORD, "Move to the next word")
{
    char* lb;
    struct Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (ctx.buf->doc->firstLine == NULL)
        return;

    for (i = 0; i < n; i++) {
        pline = ctx.buf->doc->currentLine;
        ppos = ctx.buf->doc->pos;

        if (!doc_next_nonnull_line(ctx.buf->doc, ctx.buf->doc->currentLine))
            goto end;

        l = ctx.buf->doc->currentLine;
        lb = l->lineBuf;
        while (ctx.buf->doc->pos < l->len && is_wordchar(getChar(&lb[ctx.buf->doc->pos])))
            nextChar(ctx.buf->doc->pos, l);

        while (1) {
            while (ctx.buf->doc->pos < l->len && !is_wordchar(getChar(&lb[ctx.buf->doc->pos])))
                nextChar(ctx.buf->doc->pos, l);
            if (ctx.buf->doc->pos < l->len)
                break;
            if (!doc_next_nonnull_line(ctx.buf->doc, ctx.buf->doc->currentLine->next)) {
                ctx.buf->doc->currentLine = pline;
                ctx.buf->doc->pos = ppos;
                goto end;
            }
            ctx.buf->doc->pos = 0;
            l = ctx.buf->doc->currentLine;
            lb = l->lineBuf;
        }
    }
end:
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(pgFore, NEXT_PAGE, "Scroll down one page")
{
    if (getRuntime()->vi_prec_num)
        doc_nscroll(ctx.buf->doc, searchKeyNum() * (ctx.buf->doc->LINES - 1));
    else
        doc_nscroll(ctx.buf->doc, getRuntime()->prec_num ? searchKeyNum() : searchKeyNum() * (ctx.buf->doc->LINES - 1));
}

DEFUN(pgBack, PREV_PAGE, "Scroll up one page")
{
    if (getRuntime()->vi_prec_num)
        doc_nscroll(ctx.buf->doc, -searchKeyNum() * (ctx.buf->doc->LINES - 1));
    else
        doc_nscroll(ctx.buf->doc, -(getRuntime()->prec_num ? searchKeyNum() : searchKeyNum() * (ctx.buf->doc->LINES - 1)));
}

DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page")
{
    doc_nscroll(ctx.buf->doc, searchKeyNum() * (ctx.buf->doc->LINES / 2 - 1));
}

DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page")
{
    doc_nscroll(ctx.buf->doc, -searchKeyNum() * (ctx.buf->doc->LINES / 2 - 1));
}

DEFUN(lup1, UP, "Scroll the screen up one line")
{
    doc_nscroll(ctx.buf->doc, searchKeyNum());
}

DEFUN(ldown1, DOWN, "Scroll the screen down one line")
{
    doc_nscroll(ctx.buf->doc, -searchKeyNum());
}

DEFUN(ctrCsrV, CENTER_V, "Center on cursor line")
{
    if (!ctx.buf->doc->firstLine)
        return;
    int offsety = /*ctx.buf->doc.LINES / 2*/ -ctx.buf->doc->cursorY;
    if (offsety != 0) {
        ctx.buf->doc->topLine = doc_lineSkip(ctx.buf->doc, ctx.buf->doc->topLine, -offsety);
        doc_arrangeLine(ctx.buf->doc);
    }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column")
{
    if (!ctx.buf->doc->firstLine)
        return;
    int offsetx = ctx.buf->doc->cursorX - ctx.buf->doc->COLS / 2;
    if (offsetx != 0) {
        doc_columnSkip(ctx.buf->doc, offsetx);
        doc_arrangeCursor(ctx.buf->doc);
    }
}

DEFUN(shiftl, SHIFT_LEFT, "Shift screen left")
{
    if (!ctx.buf->doc->firstLine)
        return;
    int column = ctx.buf->doc->currentColumn;
    doc_columnSkip(ctx.buf->doc, searchKeyNum() * (-ctx.buf->doc->COLS + 1) + 1);
    doc_shiftvisualpos(ctx.buf->doc, ctx.buf->doc->currentColumn - column);
}

DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;
    int column = ctx.buf->doc->currentColumn;
    doc_columnSkip(ctx.buf->doc, searchKeyNum() * (ctx.buf->doc->COLS - 1) - 1);
    doc_shiftvisualpos(ctx.buf->doc, ctx.buf->doc->currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right")
{
    struct Line* l = ctx.buf->doc->currentLine;
    if (l == NULL)
        return;
    int n = searchKeyNum();
    for (int j = 0; j < n; j++) {
        int column = ctx.buf->doc->currentColumn;
        doc_columnSkip(ctx.buf->doc, 1);
        if (column == ctx.buf->doc->currentColumn)
            break;
        doc_shiftvisualpos(ctx.buf->doc, 1);
    }
}

DEFUN(col1L, LEFT, "Shift screen one column left")
{
    struct Line* l = ctx.buf->doc->currentLine;
    if (l == NULL)
        return;
    int n = searchKeyNum();
    for (int j = 0; j < n; j++) {
        if (ctx.buf->doc->currentColumn == 0)
            break;
        doc_columnSkip(ctx.buf->doc, -1);
        doc_shiftvisualpos(ctx.buf->doc, -1);
    }
}

//
// anchor
//
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;

    struct HmarkerList* hl = ctx.buf->doc->hmarklist;
    if (!hl || hl->nmark == 0)
        return;

    int hseq = 0;
    if (getRuntime()->prec_num > hl->nmark)
        hseq = hl->nmark - 1;
    else if (getRuntime()->prec_num > 0)
        hseq = getRuntime()->prec_num - 1;

    struct BufferPoint* po;
    struct Anchor* an;
    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = al_retrieve(&ctx.buf->doc->href, (struct BufferPoint) { .line = po->line, .pos = po->pos });
        if (an == NULL)
            an = al_retrieve(&ctx.buf->doc->formitem, (struct BufferPoint) { .line = po->line, .pos = po->pos });
        hseq++;
    } while (an == NULL);

    doc_gotoLine(ctx.buf->doc, po->line);
    ctx.buf->doc->pos = po->pos;
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(lastA, LINK_END, "Move to the last hyperlink")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;

    struct HmarkerList* hl = ctx.buf->doc->hmarklist;
    if (!hl || hl->nmark == 0)
        return;

    int hseq;
    if (getRuntime()->prec_num >= hl->nmark)
        hseq = 0;
    else if (getRuntime()->prec_num > 0)
        hseq = hl->nmark - getRuntime()->prec_num;
    else
        hseq = hl->nmark - 1;

    struct BufferPoint* po;
    struct Anchor* an;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = al_retrieve(&ctx.buf->doc->href, (struct BufferPoint) { .line = po->line, .pos = po->pos });
        if (an == NULL)
            an = al_retrieve(&ctx.buf->doc->formitem, (struct BufferPoint) { .line = po->line, .pos = po->pos });
        hseq--;
    } while (an == NULL);

    doc_gotoLine(ctx.buf->doc, po->line);
    ctx.buf->doc->pos = po->pos;
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(nthA, LINK_N, "Go to the nth link")
{
    struct HmarkerList* hl = ctx.buf->doc->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;

    int n = searchKeyNum();
    if (n < 0 || n > hl->nmark)
        return;

    if (ctx.buf->doc->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    po = hl->marks + n - 1;
    an = al_retrieve(&ctx.buf->doc->href, (struct BufferPoint) { .line = po->line, .pos = po->pos });
    if (an == NULL)
        an = al_retrieve(&ctx.buf->doc->formitem, (struct BufferPoint) { .line = po->line, .pos = po->pos });
    if (an == NULL)
        return;

    doc_gotoLine(ctx.buf->doc, po->line);
    ctx.buf->doc->pos = po->pos;
    doc_arrangeCursor(ctx.buf->doc);
}

DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink")
{
    doc_nextA(ctx.buf->doc, false, buf_baseUrl(ctx.buf));
}

DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink")
{
    doc_prevA(ctx.buf->doc, false, buf_baseUrl(ctx.buf));
}

DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink")
{
    doc_nextA(ctx.buf->doc, true, buf_baseUrl(ctx.buf));
}

DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink")
{
    doc_prevA(ctx.buf->doc, true, buf_baseUrl(ctx.buf));
}

DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink")
{
    doc_nextX(ctx.buf->doc, -1, 0);
}

DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink")
{
    doc_nextX(ctx.buf->doc, -1, -1);
}

DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink")
{
    doc_nextX(ctx.buf->doc, 1, 0);
}

DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink")
{
    doc_nextX(ctx.buf->doc, 1, 1);
}

DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink")
{
    doc_nextY(ctx.buf->doc, 1);
}

DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink")
{
    doc_nextY(ctx.buf->doc, -1);
}

//
// search
//

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward")
{
    srch(ctx, forwardSearch, "Forward: ");
}

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(ctx, backwardSearch, "Backward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(ctx, forwardSearch, "I-search: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(ctx, backwardSearch, "I-search backward: ");
}

DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward")
{
    srch_nxtprv(ctx, 0);
}

DEFUN(srchprv, SEARCH_PREV, "Continue search backward")
{
    srch_nxtprv(ctx, 1);
}

DEFUN(_mark, MARK, "Set/unset mark")
{
    if (!getRuntime()->use_mark)
        return;
    if (ctx.buf->doc->firstLine == NULL)
        return;
    struct Line* l = ctx.buf->doc->currentLine;
    l->propBuf[ctx.buf->doc->pos] ^= PE_MARK;
}

DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    if (!getRuntime()->use_mark)
        return;
    if (ctx.buf->doc->firstLine == NULL)
        return;
    int i = ctx.buf->doc->pos + 1;
    struct Line* l = ctx.buf->doc->currentLine;
    if (i >= l->len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->len; i++) {
            if (l->propBuf[i] & PE_MARK) {
                ctx.buf->doc->currentLine = l;
                ctx.buf->doc->pos = i;
                doc_arrangeCursor(ctx.buf->doc);
                return;
            }
        }
        l = l->next;
        i = 0;
    }
    disp_message("No mark exist after here", TRUE);
}

DEFUN(prevMk, PREV_MARK, "Go to the previous mark")
{
    if (!getRuntime()->use_mark)
        return;
    if (ctx.buf->doc->firstLine == NULL)
        return;
    int i = ctx.buf->doc->pos - 1;
    struct Line* l = ctx.buf->doc->currentLine;
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->propBuf[i] & PE_MARK) {
                ctx.buf->doc->currentLine = l;
                ctx.buf->doc->pos = i;
                doc_arrangeCursor(ctx.buf->doc);
                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    disp_message("No mark exist before here", TRUE);
}

DEFUN(reMark, REG_MARK, "Mark all occurences of a pattern")
{
    static const char* MarkString = NULL;

    if (!getRuntime()->use_mark)
        return;
    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist("(Mark)Regexp: ", MarkString, getRuntime()->TextHist);
        if (str == NULL || *str == '\0') {
            return;
        }
    }
    str = conv_search_string(str, getRuntime()->DisplayCharset, ctx.buf->doc->charset);

    if ((str = regexCompile(str, 1)) != NULL) {
        disp_message(str, TRUE);
        return;
    }
    MarkString = str;
    for (struct Line* l = ctx.buf->doc->firstLine; l != NULL; l = l->next) {
        const char* p = l->lineBuf;
        for (;;) {
            if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1) {
                const char *p1, *p2;
                matchedPosition(&p1, &p2);
                l->propBuf[p1 - l->lineBuf] |= PE_MARK;
                p = p2;
            } else
                break;
        }
    }
}

//
// image
//
DEFUN(followI, VIEW_IMAGE, "Display image in viewer")
{
    _followI(false);
}

DEFUN(svI, SAVE_IMAGE, "Save inline image")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    _followI(true);
}

//
// load
//
DEFUN(ldfile, LOAD, "Open local file in a new buffer")
{
    const char* fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        fn = inputFilenameHist("(Load)Filename? ", NULL, getRuntime()->LoadHist);
    }
    if (fn == NULL || *fn == '\0') {
        return;
    }
    fn = conv_to_system(fn);

    struct Content* content = get_content_cache(file_to_url(fn), NULL,
        (struct LoadOption) {
            .base_url = NULL,
            .referer = NO_REFERER,
            .flag = 0,
        });
    if (content->content_type == NULL) {
        char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        disp_err_message(emsg, FALSE);
        return;
    }
    struct Buffer* buf = buf_new(content);
    tab_push_buffer(CurrentTab(), buf);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    const char* lang = getRuntime()->AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    struct Content* content = get_content_cache(tmp->ptr, NULL,
        (struct LoadOption) {
            .base_url = NULL,
            .referer = NO_REFERER,
            0,
        });
    if (!content->content_type) {
        Str emsg = Sprintf("Can't load %s", conv_from_system(tmp->ptr));
        disp_err_message(emsg->ptr, false);
        return;
    }
    struct Buffer* buf = buf_new(content);
    tab_push_buffer(CurrentTab(), buf);
}

DEFUN(selBuf, SELECT, "Display buffer-stack panel")
{
    bool ok = FALSE;
    do {
        char cmd;
        struct Buffer* buf = selectBuffer(ctx.tab->firstBuffer, ctx.buf, &cmd);
        switch (cmd) {
        case 'B':
            ok = TRUE;
            break;
        case '\n':
        case ' ':
            ctx.tab->currentBuffer = buf;
            ok = TRUE;
            break;
        case 'D':
            tab_delBuffer(ctx.tab, buf);
            if (ctx.tab->firstBuffer == NULL) {
                // No more buffer
                ctx.tab->firstBuffer = buf_new(NULL);
                ctx.tab->currentBuffer = ctx.tab->firstBuffer;
            }
            break;
        case 'q':
            qquitfm(ctx);
            break;
        case 'Q':
            quitfm(ctx);
            break;
        }
    } while (!ok);

    for (struct Buffer* buf = ctx.tab->firstBuffer; buf != NULL; buf = buf->nextBuffer) {
        if (buf == ctx.tab->currentBuffer)
            continue;
        deleteImage(buf);
        if (getRuntime()->clear_buffer)
            tmpClearBuffer(buf);
    }
}

DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    struct FollowResult res = buf_followA(ctx.buf,
        (struct FollowOption) { .on_target = true, .do_download = false });
    if (!res.new_buf) {
        return;
    }

    if (getRuntime()->check_target
        && getRuntime()->open_tab_blank
        && res.anchor->target
        && (!strcasecmp(res.anchor->target, "_new") || !strcasecmp(res.anchor->target, "_blank"))) {
        tabs_append(res.new_buf);
    } else {
        tab_push_buffer(ctx.tab, res.new_buf);
    }
}

DEFUN(submitForm, SUBMIT, "Submit form")
{
    buf_followForm(ctx.buf,
        (struct FollowOption) { .on_target = true, .do_download = false }, true);
}

DEFUN(nextBf, NEXT, "Switch to the next buffer")
{
    for (int i = 0; i < PREC_NUM; i++) {
        struct Buffer* buf = prevBuffer(ctx.tab->firstBuffer, Currentbuf);
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
}

DEFUN(prevBf, PREV, "Switch to the previous buffer")
{
    for (int i = 0; i < PREC_NUM; i++) {
        struct Buffer* buf = Currentbuf->nextBuffer;
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
}

DEFUN(backBf, BACK, "Close current buffer and return to the one below in stack")
{
    tab_back(ctx.tab);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    tab_delBuffer(ctx.tab, ctx.buf);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar")
{
    int i = 0;
    struct TabBuffer* tab = CurrentTab();
    for (; tab && i < PREC_NUM; tab = tab->nextTab, i++)
        ;
    moveTab(CurrentTab(), tab ? tab : LastTab(), TRUE);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar")
{
    struct TabBuffer* tab = CurrentTab();
    int i = 0;
    for (; tab && i < PREC_NUM;
        tab = tab->prevTab, i++)
        ;
    moveTab(CurrentTab(), tab ? tab : FirstTab(), FALSE);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel")
{
    assert(false);
    // download_panel();
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    if (!Currentbuf->doc->firstLine)
        return;
    struct DocumentPos* pos = ctx.buf->doc->undo;
    if (!pos || !pos->prev)
        return;
    for (int i = 0; i < PREC_NUM && pos->prev; i++, pos = pos->prev)
        ;
    doc_resetPos(ctx.buf->doc, pos);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    if (!Currentbuf->doc->firstLine)
        return;
    struct DocumentPos* pos = ctx.buf->doc->undo;
    if (!pos || !pos->next)
        return;
    for (int i = 0; i < PREC_NUM && pos->next; i++, pos = pos->next)
        ;
    doc_resetPos(ctx.buf->doc, pos);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen")
{
    if (Currentbuf->doc->firstLine == NULL)
        return;
    Currentbuf->doc->currentLine = doc_lineSkip(Currentbuf->doc, Currentbuf->doc->topLine, 0);
    doc_arrangeLine(Currentbuf->doc);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen")
{
    if (Currentbuf->doc->firstLine == NULL)
        return;
    int offsety = (Currentbuf->doc->LINES - 1) / 2;
    Currentbuf->doc->currentLine = currentLineSkip(Currentbuf->doc->topLine, offsety);
    doc_arrangeLine(Currentbuf->doc);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen")
{
    if (Currentbuf->doc->firstLine == NULL)
        return;
    int offsety = Currentbuf->doc->LINES - 1;
    Currentbuf->doc->currentLine = currentLineSkip(Currentbuf->doc->topLine, offsety);
    doc_arrangeLine(Currentbuf->doc);
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab")
{
    struct Content* content = goURL0(ctx.buf, "Goto relative URL on new tab: ", true);
    if (content) {
        struct Buffer* buf = buf_new(content);
        tabs_append(buf);
    }
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab")
{
    struct Content* content = goURL0(ctx.buf, "Goto relative URL on new tab: ", false);
    if (content) {
        struct Buffer* buf = buf_new(content);
        tabs_append(buf);
    }
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab")
{
    struct FollowResult res = buf_followA(ctx.buf, (struct FollowOption) { 0 });
    if (res.new_buf) {
        tabs_append(res.new_buf);
    }
}

DEFUN(nextT, NEXT_TAB, "Switch to the next tab")
{
    tabs_next(PREC_NUM);
}

DEFUN(prevT, PREV_TAB, "Switch to the previous tab")
{
    tabs_prev(PREC_NUM);
}

DEFUN(closeT, CLOSE_TAB, "Close tab")
{
    if (nTab() <= 1)
        return;
    struct TabBuffer* tab;
    if (getRuntime()->prec_num)
        tab = numTab(PREC_NUM);
    else
        tab = CurrentTab();
    if (tab)
        tabs_delete(tab);
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)")
{
    tabs_append(NULL);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer")
{
    struct Content* content = goURL0(ctx.buf, "Goto URL: ", FALSE);
    if (content) {
        struct Buffer* new_buf = buf_new(content);
        tab_push_buffer(ctx.tab, new_buf);
    }
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer")
{
    const char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        url = skip_blanks(url);
        url = url_encode(url, NULL, 0);
        struct Url p_url;
        parseURL2(url, &p_url, NULL);
        pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
        struct Content* content = get_content_cache(url, NULL, (struct LoadOption) { .base_url = NULL, .referer = NULL });
        if (content) {
            struct Buffer* buf = buf_new(content);
            tab_push_buffer(ctx.tab, buf);
            pushHashHist(getRuntime()->URLHist, parsedURL2Str(&Currentbuf->content->url)->ptr);
        }
    }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address")
{
    struct Content* content = goURL0(ctx.buf, "Goto relative URL: ", TRUE);
    if (content) {
        struct Buffer* new_buf = buf_new(content);
        tab_push_buffer(ctx.tab, new_buf);
    }
}

DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks")
{
    struct Content* content = get_content_cache(getRuntime()->BookmarkFile, NULL,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER });
    if (content) {
        struct Buffer* buf = buf_new(content);
        tab_push_buffer(ctx.tab, buf);
    }
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks")
{
    Str tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                      "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(getRuntime()->BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&Currentbuf->content->url)))->ptr,

        (Str_form_quote(wc_conv_strict(Currentbuf->doc->title,
             getRuntime()->InnerCharset,
             getRuntime()->BookmarkCharset)))
            ->ptr,
        wc_ces_to_charset(getRuntime()->BookmarkCharset));

    struct FormList* request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    *request = (struct FormList) {
        .body = tmp->ptr,
        .length = tmp->length,
    };
    struct Content* content = get_content_cache("file:///$LIB/" W3MBOOKMARK_CMDNAME, request,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER });
    if (content) {
        struct Buffer* buf = buf_new(content);
        tab_push_buffer(ctx.tab, buf);
    }
}

DEFUN(ldOpt, OPTIONS, "Display options setting panel")
{
    Str str = opt_load_panel();
    struct Buffer* new_buf = loadHTMLString(str);
    tab_push_buffer(ctx.tab, new_buf);
    buf_set_link(ctx.buf, new_buf, BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            char* v = opt_get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, getRuntime()->TextHist);
        if (opt == NULL || *opt == '\0') {
            return;
        }
    }
    if (opt_set_param_option(opt))
        sync_with_option();
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages")
{
    struct Buffer* new_buf = message_list_panel();
    buf_set_link(ctx.buf, new_buf, BP_NO_URL, LB_NOLINK);
    tab_push_buffer(ctx.tab, new_buf);
}

DEFUN(pginfo, INFO, "Display information about the current document")
{
    Str tmp = page_info_panel(ctx.buf);
    struct Buffer* new_buf = loadHTMLString(tmp);
    buf_set_link(ctx.buf, new_buf, BP_NORMAL, LB_INFO);
    tab_push_buffer(ctx.tab, new_buf);
}

DEFUN(linkMn, LINK_MENU, "Pop up link element menu")
{
    struct LinkList* l = link_menu(ctx.buf);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(Currentbuf, l->url + 1);
        return;
    }
    parseURL2(l->url, &p_url, buf_baseUrl(Currentbuf));
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content* content = get_content_cache(l->url, NULL,
        (struct LoadOption) {
            .base_url = buf_baseUrl(Currentbuf),
            .referer = parsedURL2Str(&Currentbuf->content->url)->ptr });
    if (content) {
        struct Buffer* buf = buf_new(content);
        tab_push_buffer(ctx.tab, buf);
    }
}

typedef struct Anchor* (*BufferMenuFunc)(struct Buffer*);

void anchorMn(struct DefunContext ctx, BufferMenuFunc menu_func, bool go)
{
    if (ctx.buf->doc->href.nanchor == 0 || !ctx.buf->doc->hmarklist)
        return;

    struct Anchor* a = menu_func(ctx.buf);
    if (!a || a->hseq < 0)
        return;

    struct BufferPoint* po = &ctx.buf->doc->hmarklist->marks[a->hseq];
    doc_gotoLine(ctx.buf->doc, po->line);
    ctx.buf->doc->pos = po->pos;
    doc_arrangeCursor(ctx.buf->doc);
    if (go) {
        followA(ctx);
    }
}

DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu")
{
    anchorMn(ctx, accesskey_menu, TRUE);
}

DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to")
{
    anchorMn(ctx, list_menu, true);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks")
{
    anchorMn(ctx, list_menu, false);
}

DEFUN(linkLst, LIST, "Show all URLs referenced")
{
    Str page = link_list_panel(buf_baseUrl(ctx.buf), ctx.buf->doc);
    if (page) {
        struct Buffer* new_buf = loadHTMLString(page);
        buf_set_link(ctx.buf, new_buf, BP_NORMAL, LB_NOLINK);
        tab_push_buffer(ctx.tab, new_buf);
    }
}

DEFUN(cooLst, COOKIE, "View cookie list")
{
    struct Buffer* new_buf = cookie_list_panel();
    if (new_buf) {
        buf_set_link(ctx.buf, new_buf, BP_NO_URL, LB_NOLINK);
        tab_push_buffer(ctx.tab, new_buf);
    }
}

DEFUN(ldHist, HISTORY, "Show browsing history")
{
    struct Buffer* new_buf = historyBuffer(getRuntime()->URLHist);
    buf_set_link(ctx.buf, new_buf, BP_NO_URL, LB_NOLINK);
    tab_push_buffer(ctx.tab, new_buf);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    buf_followA(ctx.buf, (struct FollowOption) { .on_target = true, .do_download = false });
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */

    char* file = searchKeyData();
    char* qfile = NULL;
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, getRuntime()->SaveHist);
        if (qfile == NULL || *qfile == '\0') {
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);

    FILE* f;
    bool is_pipe;
    if (*file == '|') {
        is_pipe = TRUE;
        f = popen(file + 1, "w");
    } else {
        if (qfile) {
            file = unescape_spaces(Strnew_charp(qfile))->ptr;
            file = conv_to_system(file);
        }
        file = expandPath(file);
        if (checkOverWrite(file) < 0) {
            return;
        }
        f = fopen(file, "w");
        is_pipe = FALSE;
    }
    if (f == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
        disp_err_message(emsg, TRUE);
        return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    if (is_pipe)
        pclose(f);
    else
        fclose(f);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source")
{
    if (Currentbuf->content->sourcefile == NULL)
        return;
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    getRuntime()->PermitSaveToPipe = TRUE;
    const char* file;
    if (Currentbuf->content->url.scheme == SCM_LOCAL)
        file = conv_from_system(guess_save_name(NULL,
            Currentbuf->content->url.real_file));
    else
        file = guess_save_name(Currentbuf->content, Currentbuf->content->url.file);
    doFileCopy(Currentbuf->content->sourcefile, file);
    getRuntime()->PermitSaveToPipe = FALSE;
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address")
{
    _peekURL(ctx.buf, false);
}

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address")
{
    _peekURL(ctx.buf, true);
}

DEFUN(curURL, PEEK, "Show current address")
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;

    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (getRuntime()->CurrentKey == getRuntime()->prev_key && s != NULL) {
        if (s->length - offset >= TTY_COLS())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL(ctx.buf);
        if (getRuntime()->DecodeURL)
            s = Strnew_charp(url_decode2(NULL, NULL, s->ptr));
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (TTY_COLS() - 1))
        offset = (n - 1) * (TTY_COLS() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], TRUE);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed")
{
    struct Buffer* buf;

    if (Currentbuf->content->content_type == NULL || Currentbuf->bufferprop & BP_FRAME)
        return;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL || (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
        Currentbuf = buf;
        return;
    }
    if (Currentbuf->content->sourcefile == NULL) {
        // if (Currentbuf->pagerSource && !strcasecmp(Currentbuf->type, "text/plain")) {
        //     wc_ces old_charset;
        //     wc_bool old_fix_width_conv;
        //
        //     FILE* f;
        //     Str tmpf = tmpfname(TMPF_SRC, NULL);
        //     f = fopen(tmpf->ptr, "w");
        //     if (f == NULL)
        //         return;
        //
        //     old_charset = getRuntime()->DisplayCharset;
        //     old_fix_width_conv = WcOption.fix_width_conv;
        //     getRuntime()->DisplayCharset = (Currentbuf->document_charset != WC_CES_US_ASCII)
        //         ? Currentbuf->document_charset
        //         : 0;
        //     WcOption.fix_width_conv = WC_FALSE;
        //
        //     saveBufferBody(Currentbuf, f, TRUE);
        //
        //     getRuntime()->DisplayCharset = old_charset;
        //     WcOption.fix_width_conv = old_fix_width_conv;
        //
        //     fclose(f);
        //     Currentbuf->sourcefile = tmpf->ptr;
        // }
        // else
        {
            return;
        }
    }

    buf = buf_new(NULL);

    if (is_html_type(Currentbuf->content->content_type)) {
        buf->content->content_type = "text/plain";
        if (Currentbuf->content->content_type && is_html_type(Currentbuf->content->content_type))
            buf->content->content_type = "text/plain";
        else
            buf->content->content_type = Currentbuf->content->content_type;
        buf->doc->title = Sprintf("source of %s", Currentbuf->doc->title)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_SOURCE] = buf;
    } else if (!strcasecmp(Currentbuf->content->content_type, "text/plain")) {
        buf->content->content_type = "text/html";
        if (Currentbuf->content->content_type && !strcasecmp(Currentbuf->content->content_type, "text/plain"))
            buf->content->content_type = "text/html";
        else
            buf->content->content_type = Currentbuf->content->content_type;
        buf->doc->title = Sprintf("HTML view of %s",
            Currentbuf->doc->title)
                              ->ptr;
        buf->linkBuffer[LB_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
    } else {
        return;
    }
    buf->content->url = Currentbuf->content->url;
    buf->content->filename = Currentbuf->content->filename;
    buf->content->sourcefile = Currentbuf->content->sourcefile;
    buf->content->header_source = Currentbuf->content->header_source;
    // buf->search_header = Currentbuf->search_header;
    buf->doc->charset = Currentbuf->doc->charset;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;
    reshapeBuffer(buf);
    tab_push_buffer(CurrentTab(), buf);
}

DEFUN(reload, RELOAD, "Load current document anew")
{
    if (ctx.buf->bufferprop & BP_INTERNAL) {
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (ctx.buf->content->url.scheme == SCM_LOCAL && !strcmp(ctx.buf->content->url.file, "-")) {
        // file is std input
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }
    struct Buffer sbuf;
    copyBuffer(&sbuf, ctx.buf);
    int multipart = 0;
    struct FormList* request;
    if (ctx.buf->doc->form_submit) {
        request = ctx.buf->doc->form_submit->parent;
        if (request->method == FORM_METHOD_POST
            && request->enctype == FORM_ENCTYPE_MULTIPART) {
            struct stat st;
            multipart = 1;
            query_from_followform(ctx.buf, ctx.buf->doc->form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    } else {
        request = NULL;
    }
    Str url = parsedURL2Str(&ctx.buf->content->url);
    message("Reloading...");
    enum wc_ces old_charset = getRuntime()->DocumentCharset;
    if (ctx.buf->doc->charset != WC_CES_US_ASCII)
        getRuntime()->DocumentCharset = ctx.buf->doc->charset;
    // SearchHeader = ctx.buf->search_header;
    getRuntime()->DefaultType = ctx.buf->content->content_type;
    struct Content* content = get_content_cache(url->ptr, request,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = RG_NOCACHE });
    if (!content) {
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    struct Buffer* new_buf = buf_new(content);
    getRuntime()->DocumentCharset = old_charset;
    // SearchHeader = FALSE;
    getRuntime()->DefaultType = NULL;

    if (multipart)
        unlink(request->body);
    tab_repBuffer(ctx.tab, ctx.buf, new_buf);
    if ((new_buf->content->content_type != NULL) && (sbuf.content->content_type != NULL) && ((!strcasecmp(new_buf->content->content_type, "text/plain") && is_html_type(sbuf.content->content_type)) || (is_html_type(new_buf->content->content_type) && !strcasecmp(sbuf.content->content_type, "text/plain")))) {
        vwSrc(ctx);
        tab_deleteBuffer(ctx.tab, new_buf);
    }
    // Currentbuf->search_header = sbuf.search_header;
    ctx.buf->doc->form_submit = sbuf.doc->form_submit;
    if (ctx.buf->doc->firstLine) {
        COPY_BUFROOT(ctx.buf->doc, sbuf.doc);
        doc_restorePosition(ctx.buf->doc, sbuf.doc);
    }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document")
{
    reshapeBuffer(Currentbuf);
}

DEFUN(setAlarm, ALARM, "Set alarm")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("(Alarm)sec command: ", "", getRuntime()->TextHist);
        if (data == NULL) {
            return;
        }
    }
    set_alarm(data);
}

DEFUN(reinit, REINIT, "Reload configuration file")
{
    char* resource = searchKeyData();

    if (resource == NULL) {
        init_rc();
        sync_with_option();
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option();
        return;
    }

    if (!strcasecmp(resource, "COOKIE")) {
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "KEYMAP")) {
        keymap_init(true);
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
        initMimeTypes();
        return;
    }

    if (!strcasecmp(resource, "URIMETHODS")) {
        initURIMethods();
        return;
    }

    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, FALSE);
}

DEFUN(defKey, DEFINE_KEY, "Define a binding between a key stroke combination and a command")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("Key definition: ", "", getRuntime()->TextHist);
        if (data == NULL || *data == '\0') {
            return;
        }
    }
    keymap_parseLine(allocStr(data, -1), -1, TRUE);
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)")
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("command [; ...]: ", "", getRuntime()->TextHist);
        if (data == NULL) {
            return;
        }
    }
    /* data: FUNC [DATA] [; FUNC [DATA] ...] */
    while (*data) {
        data = skip_blanks(data);
        if (*data == ';') {
            data++;
            continue;
        }
        char* p = getWord(&data);
        DefunFunc func = keymap_fromName(p);
        if (!func)
            break;
        p = getQWord(&data);
        getRuntime()->CurrentKey = -1;
        getRuntime()->CurrentKeyData = NULL;
        getRuntime()->CurrentCmdData = *p ? p : NULL;
        func(ctx);
        getRuntime()->CurrentCmdData = NULL;
    }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)")
{
    execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
    "Execute dictionary command for word at cursor")
{
    execdict(GetWord(Currentbuf));
}

DEFUN(docCSet, CHARSET, "Change the character encoding for the current document")
{
    char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
            wc_ces_to_charset(Currentbuf->doc->charset));

    enum wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        return;
    }
    _docCSet(charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding")
{
    char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
            wc_ces_to_charset(getRuntime()->DocumentCharset));
    enum wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        getRuntime()->DocumentCharset = charset;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks")
{
    chkURLBuffer(Currentbuf);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink")
{
    int spos, epos;
    const char* p = doc_getCurWord(ctx.buf->doc, &spos, &epos);
    if (p == NULL)
        return;
    doc_reAnchorWord(buf_baseUrl(ctx.buf), ctx.buf->doc, ctx.buf->doc->currentLine, spos, epos);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames")
{
}

DEFUN(extbrz, EXTERN, "Display using an external browser")
{
    if (Currentbuf->bufferprop & BP_INTERNAL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (Currentbuf->content->url.scheme == SCM_LOCAL && !strcmp(Currentbuf->content->url.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(parsedURL2Str(&Currentbuf->content->url)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser")
{
    if (ctx.buf->doc->firstLine == NULL)
        return;
    struct Anchor* a = doc_retrieveCurrentAnchor(ctx.buf->doc);
    if (a == NULL)
        return;
    struct Url pu;
    parseURL2(a->url, &pu, buf_baseUrl(ctx.buf));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document")
{
    struct Line* l = Currentbuf->doc->currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;

    if (l != NULL) {
        cur = l->real_linenumber;
        col = l->bwidth + Currentbuf->doc->currentColumn + Currentbuf->doc->cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        len = l->bwidth + l->width;
    }
    if (Currentbuf->doc->lastLine)
        all = Currentbuf->doc->lastLine->real_linenumber;
    // if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
    //     tmp = Sprintf("line %d col %d/%d", cur, col, len);
    // else
    tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
        (int)((double)cur * 100.0 / (double)(all ? all : 1)
            + 0.5),
        col, len);
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->doc->charset));

    disp_message(tmp->ptr, FALSE);
}

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images")
{
    if (!getRuntime()->displayImage)
        initImage();
    if (!getRuntime()->activeImage)
        return;
    getRuntime()->displayImage = true;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->doc->image_flag = IMG_FLAG_AUTO;
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images")
{
    if (!getRuntime()->activeImage)
        return;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->doc->image_flag = IMG_FLAG_SKIP;
}

DEFUN(dispVer, VERSION, "Display the version of w3m")
{
    disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches")
{
    if (getRuntime()->WrapSearch) {
        getRuntime()->WrapSearch = FALSE;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", TRUE);
    } else {
        getRuntime()->WrapSearch = TRUE;
        /* FIXME: gettextize? */
        disp_message("Wrap search on", TRUE);
    }
}
