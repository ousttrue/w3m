#include "defun.h"
#include "tab_list.h"
#include "hmarker.h"
#include "frame.h"
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{
}

DEFUN(quitfm, ABORT EXIT, "Quit without confirmation")
{
    _quitfm(FALSE);
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
    struct HmarkerList* hl = Currentbuf->doc->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;

    int n = searchKeyNum();
    if (n < 0 || n > hl->nmark)
        return;

    if (Currentbuf->doc->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    po = hl->marks + n - 1;
    an = al_retrieve(&Currentbuf->doc->href, (struct BufferPoint) { .line = po->line, .pos = po->pos });
    if (an == NULL)
        an = al_retrieve(&Currentbuf->doc->formitem, (struct BufferPoint) { .line = po->line, .pos = po->pos });
    if (an == NULL)
        return;

    doc_gotoLine(Currentbuf->doc, po->line);
    Currentbuf->doc->pos = po->pos;
    doc_arrangeCursor(Currentbuf->doc);
}

DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink")
{
    doc_nextA(ctx.buf->doc, false, baseURL(ctx.buf));
}

DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink")
{
    doc_prevA(ctx.buf->doc, false, baseURL(ctx.buf));
}

DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink")
{
    doc_nextA(ctx.buf->doc, true, baseURL(ctx.buf));
}

DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink")
{
    doc_prevA(ctx.buf->doc, true, baseURL(ctx.buf));
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
    srch(ctx.buf->doc, forwardSearch, "Forward: ");
}

DEFUN(srchbak, SEARCH_BACK, "Search backward")
{
    srch(ctx.buf->doc, backwardSearch, "Backward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward")
{
    isrch(ctx.buf->doc, forwardSearch, "I-search: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward")
{
    isrch(ctx.buf->doc, backwardSearch, "I-search backward: ");
}

DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward")
{
    srch_nxtprv(ctx.buf->doc, 0);
}

DEFUN(srchprv, SEARCH_PREV, "Continue search backward")
{
    srch_nxtprv(ctx.buf->doc, 1);
}

DEFUN(_mark, MARK, "Set/unset mark")
{
    if (!getRuntime()->use_mark)
        return;
    if (Currentbuf->doc->firstLine == NULL)
        return;
    struct Line* l = Currentbuf->doc->currentLine;
    l->propBuf[Currentbuf->doc->pos] ^= PE_MARK;
}

DEFUN(nextMk, NEXT_MARK, "Go to the next mark")
{
    if (!getRuntime()->use_mark)
        return;
    if (Currentbuf->doc->firstLine == NULL)
        return;
    int i = Currentbuf->doc->pos + 1;
    struct Line* l = Currentbuf->doc->currentLine;
    if (i >= l->len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->len; i++) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->doc->currentLine = l;
                Currentbuf->doc->pos = i;
                doc_arrangeCursor(Currentbuf->doc);
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
    if (Currentbuf->doc->firstLine == NULL)
        return;
    int i = Currentbuf->doc->pos - 1;
    struct Line* l = Currentbuf->doc->currentLine;
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->doc->currentLine = l;
                Currentbuf->doc->pos = i;
                doc_arrangeCursor(Currentbuf->doc);
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
    for (struct Line* l = Currentbuf->doc->firstLine; l != NULL; l = l->next) {
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
            Currentbuf = buf;
            ok = TRUE;
            break;
        case 'D':
            delBuffer(buf);
            if (Firstbuf == NULL) {
                // No more buffer
                Firstbuf = buf_new(NULL);
                Currentbuf = Firstbuf;
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

    for (struct Buffer* buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            continue;
        deleteImage(buf);
        if (getRuntime()->clear_buffer)
            tmpClearBuffer(buf);
    }
}

DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer")
{
    struct FollowResult res = _followA(Currentbuf,
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
    _followForm(Currentbuf,
        (struct FollowOption) { .on_target = true, .do_download = false }, true);
}

DEFUN(nextBf, NEXT, "Switch to the next buffer")
{
    for (int i = 0; i < PREC_NUM; i++) {
        struct Buffer* buf = prevBuffer(Firstbuf, Currentbuf);
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
    struct Buffer* buf = ctx.buf->linkBuffer[LB_N_FRAME];

    if (!checkBackBuffer(Currentbuf)) {
        if (getRuntime()->close_tab_back && nTab() >= 1) {
            tabs_delete(ctx.tab);
        } else
            /* FIXME: gettextize? */
            disp_message("Can't go back...", TRUE);
        return;
    }

    delBuffer(Currentbuf);

    if (buf) {
        if (buf->doc->frameQ) {
            struct frameset* fs;
            long linenumber = buf->doc->frameQ->linenumber;
            long top = buf->doc->frameQ->top_linenumber;
            int pos = buf->doc->frameQ->pos;
            int currentColumn = buf->doc->frameQ->currentColumn;
            struct AnchorList* formitem = buf->doc->frameQ->formitem;

            fs = popFrameTree(&(buf->doc->frameQ));
            deleteFrameSet(buf->doc->frameset);
            buf->doc->frameset = fs;

            if (buf == Currentbuf) {
                rFrame(ctx);
                Currentbuf->doc->topLine = doc_lineSkip(Currentbuf->doc,
                    Currentbuf->doc->firstLine, top - 1);
                doc_gotoLine(Currentbuf->doc, linenumber);
                Currentbuf->doc->pos = pos;
                Currentbuf->doc->currentColumn = currentColumn;
                doc_arrangeCursor(Currentbuf->doc);
                formResetBuffer(Currentbuf, formitem);
            }
        } else if (getRuntime()->RenderFrame && buf == Currentbuf) {
            delBuffer(Currentbuf);
        }
    }
}

DEFUN(deletePrevBuf, DELETE_PREVBUF, "Delete previous buffer (mainly for local CGI-scripts)")
{
    struct Buffer* buf = Currentbuf->nextBuffer;
    if (buf)
        delBuffer(buf);
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
