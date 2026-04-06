#include "defun_impl.h"
#include <w3m.h>
#include "constants.h"
#include "myctype.h"
#include "form.h"
#include "func.h"
#include "frame.h"
#include "menu.h"
#include "terms.h"
#include "indep.h"
#include "anchor.h"
#include "signal_util.h"
#include "downloadlist.h"
#include "alarm.h"
#include "search.h"
#include "wc_util.h"
#include "mailcap.h"
#include "maparea.h"
#include "cookie.h"
#include "etc.h"
#include "url.h"
#include "rc.h"
#include "buffer.h"
#include "local.h"
#include "image.h"
#include "global.h"
#include "history.h"
#include "line_input.h"
#include "keybind.h"
#include "main.h"
#include "proto.h"
#include "display.h"
#include "util.h"
#include "regex.h"

#include <libwc/charset.h>

#include <strings.h>
#include <signal.h>
#include <unistd.h>

void nulcmd(struct CmdArgs args)
{ /* do nothing */
}

void escmap(struct CmdArgs args)
{
    char c = getch();
    if (IS_ASCII(c))
        escKeyProc((int)c, K_ESC, EscKeymap);
}

void escbmap(struct CmdArgs args)
{
    char c;
    c = getch();
    if (IS_DIGIT(c)) {
        escdmap(c);
        return;
    }
    if (IS_ASCII(c))
        escKeyProc((int)c, K_ESCB, EscBKeymap);
}

void multimap(struct CmdArgs args)
{
    char c;
    c = getch();
    if (IS_ASCII(c)) {
        CurrentKey = K_MULTI | (CurrentKey << 16) | c;
        escKeyProc((int)c, 0, NULL);
    }
}

void pgFore(struct CmdArgs args)
{
    if (vi_prec_num)
        nscroll(searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
    else
        nscroll(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->LINES - 1), prec_num ? B_SCROLL : B_NORMAL);
}

void pgBack(struct CmdArgs args)
{
    if (vi_prec_num)
        nscroll(-searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
    else
        nscroll(-(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->LINES - 1)), prec_num ? B_SCROLL : B_NORMAL);
}

void hpgFore(struct CmdArgs args)
{
    nscroll(searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
}

void hpgBack(struct CmdArgs args)
{
    nscroll(-searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
}

void lup1(struct CmdArgs args)
{
    nscroll(searchKeyNum(), B_SCROLL);
}

void ldown1(struct CmdArgs args)
{
    nscroll(-searchKeyNum(), B_SCROLL);
}

void ctrCsrV(struct CmdArgs args)
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = /*Currentbuf->LINES / 2*/ -Currentbuf->cursorY;
    if (offsety != 0) {
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine, -offsety, FALSE);
        arrangeLine(Currentbuf);
        displayBuffer(Currentbuf, B_NORMAL);
    }
}

void ctrCsrH(struct CmdArgs args)
{
    int offsetx;
    if (Currentbuf->firstLine == NULL)
        return;
    offsetx = Currentbuf->cursorX - Currentbuf->COLS / 2;
    if (offsetx != 0) {
        columnSkip(Currentbuf, offsetx);
        arrangeCursor(Currentbuf);
        displayBuffer(Currentbuf, B_NORMAL);
    }
}

void rdrwSc(struct CmdArgs args)
{
    clear();
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void srchfor(struct CmdArgs args)
{
    srch(forwardSearch, "Forward: ");
}

void isrchfor(struct CmdArgs args)
{
    isrch(forwardSearch, "I-search: ");
}

void srchbak(struct CmdArgs args)
{
    srch(backwardSearch, "Backward: ");
}

void isrchbak(struct CmdArgs args)
{
    isrch(backwardSearch, "I-search backward: ");
}

void srchnxt(struct CmdArgs args)
{
    srch_nxtprv(0);
}

void srchprv(struct CmdArgs args)
{
    srch_nxtprv(1);
}

void shiftl(struct CmdArgs args)
{
    int column;

    if (Currentbuf->firstLine == NULL)
        return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->COLS + 1) + 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

void shiftr(struct CmdArgs args)
{
    int column;

    if (Currentbuf->firstLine == NULL)
        return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->COLS - 1) - 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

void col1R(struct CmdArgs args)
{
    Buffer* buf = Currentbuf;
    struct Line* l = buf->currentLine;
    int j, column, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        column = buf->currentColumn;
        columnSkip(Currentbuf, 1);
        if (column == buf->currentColumn)
            break;
        shiftvisualpos(Currentbuf, 1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void col1L(struct CmdArgs args)
{
    Buffer* buf = Currentbuf;
    struct Line* l = buf->currentLine;
    int j, n = searchKeyNum();

    if (l == NULL)
        return;
    for (j = 0; j < n; j++) {
        if (buf->currentColumn == 0)
            break;
        columnSkip(Currentbuf, -1);
        shiftvisualpos(Currentbuf, -1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void setEnv(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
        if (env != NULL && *env != '\0')
            env = Sprintf("%s=", env)->ptr;
        env = inputStrHist("Set environ: ", env, TextHist);
        if (env == NULL || *env == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    const char* value;
    if ((value = strchr(env, '=')) != NULL && value > env) {
        const char* var = allocStr(env, value - env);
        value++;
        set_environ(var, value);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void pipeBuf(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
        return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    fclose(f);
    Buffer* buf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
    if (buf == NULL) {
        disp_message("Execution failed", TRUE);
        return;
    } else {
        buf->filename = cmd;
        buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME, conv_from_system(cmd))->ptr;
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        buf->currentURL.file = "-";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void pipesh(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    Buffer* buf = getpipe(cmd);
    if (buf == NULL) {
        disp_message("Execution failed", TRUE);
        return;
    } else {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void readsh(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
        cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    auto prevtrap = mySignal(SIGINT, intTrap);
    crmode();
    Buffer* buf = getshell(cmd);
    mySignal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL) {
        /* FIXME: gettextize? */
        disp_message("Execution failed", TRUE);
        return;
    } else {
        buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        if (buf->type == NULL)
            buf->type = "text/plain";
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void execsh(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
        cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
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
        getch();
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void ldfile(struct CmdArgs args)
{
    const char* fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
        /* FIXME: gettextize? */
        fn = inputFilenameHist("(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
        fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    cmd_loadfile(fn);
}

void ldhelp(struct CmdArgs args)
{
    const char* lang = AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
}

void movL(struct CmdArgs args)
{
    _movL(Currentbuf->COLS / 2);
}

void movL1(struct CmdArgs args)
{
    _movL(1);
}

void movD(struct CmdArgs args)
{
    _movD((Currentbuf->LINES + 1) / 2);
}

void movD1(struct CmdArgs args)
{
    _movD(1);
}

void movU(struct CmdArgs args)
{
    _movU((Currentbuf->LINES + 1) / 2);
}

void movU1(struct CmdArgs args)
{
    _movU(1);
}

void movR(struct CmdArgs args)
{
    _movR(Currentbuf->COLS / 2);
}

void movR1(struct CmdArgs args)
{
    _movR(1);
}

void movLW(struct CmdArgs args)
{
    char* lb;
    struct Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;

    for (i = 0; i < n; i++) {
        pline = Currentbuf->currentLine;
        ppos = Currentbuf->pos;

        if (prev_nonnull_line(Currentbuf->currentLine) < 0)
            goto end;

        while (1) {
            l = Currentbuf->currentLine;
            lb = l->lineBuf;
            while (Currentbuf->pos > 0) {
                int tmp = Currentbuf->pos;
                prevChar(tmp, l);
                if (is_wordchar(getChar(&lb[tmp])))
                    break;
                Currentbuf->pos = tmp;
            }
            if (Currentbuf->pos > 0)
                break;
            if (prev_nonnull_line(Currentbuf->currentLine->prev) < 0) {
                Currentbuf->currentLine = pline;
                Currentbuf->pos = ppos;
                goto end;
            }
            Currentbuf->pos = Currentbuf->currentLine->len;
        }

        l = Currentbuf->currentLine;
        lb = l->lineBuf;
        while (Currentbuf->pos > 0) {
            int tmp = Currentbuf->pos;
            prevChar(tmp, l);
            if (!is_wordchar(getChar(&lb[tmp])))
                break;
            Currentbuf->pos = tmp;
        }
    }
end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

void movRW(struct CmdArgs args)
{
    char* lb;
    struct Line *pline, *l;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
        return;

    for (i = 0; i < n; i++) {
        pline = Currentbuf->currentLine;
        ppos = Currentbuf->pos;

        if (next_nonnull_line(Currentbuf->currentLine) < 0)
            goto end;

        l = Currentbuf->currentLine;
        lb = l->lineBuf;
        while (Currentbuf->pos < l->len && is_wordchar(getChar(&lb[Currentbuf->pos])))
            nextChar(Currentbuf->pos, l);

        while (1) {
            while (Currentbuf->pos < l->len && !is_wordchar(getChar(&lb[Currentbuf->pos])))
                nextChar(Currentbuf->pos, l);
            if (Currentbuf->pos < l->len)
                break;
            if (next_nonnull_line(Currentbuf->currentLine->next) < 0) {
                Currentbuf->currentLine = pline;
                Currentbuf->pos = ppos;
                goto end;
            }
            Currentbuf->pos = 0;
            l = Currentbuf->currentLine;
            lb = l->lineBuf;
        }
    }
end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Quit */
void quitfm(struct CmdArgs args)
{
    _quitfm(FALSE);
}

/* Question and Quit */
void qquitfm(struct CmdArgs args)
{
    _quitfm(confirm_on_quit);
}

/* Select buffer */
void selBuf(struct CmdArgs args)
{
    Buffer* buf;
    int ok;
    char cmd;

    ok = FALSE;
    do {
        buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
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
                /* No more buffer */
                Firstbuf = nullBuffer();
                Currentbuf = Firstbuf;
            }
            break;
        case 'q':
            qquitfm((struct CmdArgs) { 0 });
            break;
        case 'Q':
            quitfm((struct CmdArgs) { 0 });
            break;
        }
    } while (!ok);

    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            continue;
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
void susp(struct CmdArgs args)
{
    move((LINES - 1), 0);
    clrtoeolx();
    refresh();
    fmTerm();
    signal(SIGTSTP, SIG_DFL); /* just in case */
    /*
     * Note: If susp() was called from SIGTSTP handler,
     * unblocking SIGTSTP would be required here.
     * Currently not.
     */
    kill(0, SIGTSTP); /* stop whole job, not a single process */

    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void goLine(struct CmdArgs args)
{
    const char* str = searchKeyData();
    if (prec_num)
        _goLine("^");
    else if (str)
        _goLine(str);
    else
        /* FIXME: gettextize? */
        _goLine(inputStr("Goto line: ", ""));
}

void goLineF(struct CmdArgs args)
{
    _goLine("^");
}

void goLineL(struct CmdArgs args)
{
    _goLine("$");
}

/* Go to the beginning of the line */
void linbeg(struct CmdArgs args)
{
    if (Currentbuf->firstLine == NULL)
        return;
    while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
        cursorUp0(Currentbuf, 1);
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
void linend(struct CmdArgs args)
{
    if (Currentbuf->firstLine == NULL)
        return;
    while (Currentbuf->currentLine->next
        && Currentbuf->currentLine->next->bpos)
        cursorDown0(Currentbuf, 1);
    Currentbuf->pos = Currentbuf->currentLine->len - 1;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Run editor on the current buffer */
void editBf(struct CmdArgs args)
{
    const char* fn = Currentbuf->filename;
    Str cmd;

    if (fn == NULL || Currentbuf->pagerSource != NULL || /* Behaving as a pager */
        (Currentbuf->type == NULL && Currentbuf->edit == NULL) || /* Reading shell */
        Currentbuf->real_scheme != SCM_LOCAL || !strcmp(Currentbuf->currentURL.file, "-") || /* file is std input  */
        Currentbuf->bufferprop & BP_FRAME) { /* Frame */
        disp_err_message("Can't edit other than local file", TRUE);
        return;
    }
    if (Currentbuf->edit)
        cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
            checkHeader(Currentbuf, "Content-Type:"), NULL);
    else
        cmd = myEditor(Editor, shell_quote(fn),
            cur_real_linenumber(Currentbuf));
    exec_cmd(cmd->ptr);

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    reload((struct CmdArgs) { 0 });
}

/* Run editor on the current screen */
void editScr(struct CmdArgs args)
{
    const char* tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    FILE* f = fopen(tmpf, "w");
    if (f == NULL) {
        /* FIXME: gettextize? */
        disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, TRUE);
        return;
    }
    saveBuffer(Currentbuf, f, TRUE);
    fclose(f);
    exec_cmd(myEditor(Editor, shell_quote(tmpf),
        cur_real_linenumber(Currentbuf))
            ->ptr);
    unlink(tmpf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Set / unset mark */
void _mark(struct CmdArgs args)
{
    struct Line* l;
    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    l = Currentbuf->currentLine;
    l->propBuf[Currentbuf->pos] ^= PE_MARK;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Go to next mark */
void nextMk(struct CmdArgs args)
{
    struct Line* l;
    int i;

    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    i = Currentbuf->pos + 1;
    l = Currentbuf->currentLine;
    if (i >= l->len) {
        i = 0;
        l = l->next;
    }
    while (l != NULL) {
        for (; i < l->len; i++) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->currentLine = l;
                Currentbuf->pos = i;
                arrangeCursor(Currentbuf);
                displayBuffer(Currentbuf, B_NORMAL);
                return;
            }
        }
        l = l->next;
        i = 0;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist after here", TRUE);
}

/* Go to previous mark */
void prevMk(struct CmdArgs args)
{
    struct Line* l;
    int i;

    if (!use_mark)
        return;
    if (Currentbuf->firstLine == NULL)
        return;
    i = Currentbuf->pos - 1;
    l = Currentbuf->currentLine;
    if (i < 0) {
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    while (l != NULL) {
        for (; i >= 0; i--) {
            if (l->propBuf[i] & PE_MARK) {
                Currentbuf->currentLine = l;
                Currentbuf->pos = i;
                arrangeCursor(Currentbuf);
                displayBuffer(Currentbuf, B_NORMAL);
                return;
            }
        }
        l = l->prev;
        if (l != NULL)
            i = l->len - 1;
    }
    /* FIXME: gettextize? */
    disp_message("No mark exist before here", TRUE);
}

/* Mark place to which the regular expression matches */
void reMark(struct CmdArgs args)
{
    struct Line* l;
    const char *p, *p1, *p2;

    if (!use_mark)
        return;
    const char* str = searchKeyData();
    if (str == NULL || *str == '\0') {
        str = inputStrHist("(Mark)Regexp: ", MarkString, TextHist);
        if (str == NULL || *str == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    str = conv_search_string(str, DisplayCharset);
    if ((str = regexCompile(str, 1)) != NULL) {
        disp_message(str, TRUE);
        return;
    }
    MarkString = str;
    for (l = Currentbuf->firstLine; l != NULL; l = l->next) {
        p = l->lineBuf;
        for (;;) {
            if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1) {
                matchedPosition(&p1, &p2);
                l->propBuf[p1 - l->lineBuf] |= PE_MARK;
                p = p2;
            } else
                break;
        }
    }

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* follow HREF link */
void followA(struct CmdArgs args)
{
    struct Url u;
    int x = 0, y = 0, map = 0;

    if (Currentbuf->firstLine == NULL)
        return;

    struct Anchor* a = retrieveCurrentImg(Currentbuf);
    if (a && a->image && a->image->map) {
        _followForm(FALSE);
        return;
    }
    if (a && a->image && a->image->ismap) {
        getMapXY(Currentbuf, a, &x, &y);
        map = 1;
    }
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL) {
        _followForm(FALSE);
        return;
    }
    if (*a->url == '#') { /* index within this buffer */
        gotoLabel(a->url + 1);
        return;
    }
    parseURL2(a->url, &u, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
        /* index within this buffer */
        if (u.label) {
            gotoLabel(u.label);
            return;
        }
    }
    if (handleMailto(a->url))
        return;
    const char* url = a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    if (check_target && open_tab_blank && a->target && (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
        Buffer* buf;

        _newT();
        buf = Currentbuf;
        loadLink(url, a->target, a->referer, NULL);
        if (buf != Currentbuf)
            delBuffer(buf);
        else
            deleteTab(CurrentTab);
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    }
    loadLink(url, a->target, a->referer, NULL);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* view inline image */
void followI(struct CmdArgs args)
{
    struct Anchor* a;
    Buffer* buf;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentImg(Currentbuf);
    if (a == NULL)
        return;
    /* FIXME: gettextize? */
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    buf = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        char* emsg = Sprintf("Can't load %s", a->url)->ptr;
        disp_err_message(emsg, FALSE);
    } else if (buf != NO_BUFFER) {
        pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* submit form */
void submitForm(struct CmdArgs args)
{
    _followForm(TRUE);
}

/* go to the top anchor */
void topA(struct CmdArgs args)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq = 0;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    if (prec_num > hl->nmark)
        hseq = hl->nmark - 1;
    else if (prec_num > 0)
        hseq = prec_num - 1;
    do {
        if (hseq >= hl->nmark)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
        hseq++;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
void lastA(struct CmdArgs args)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;
    int hseq;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    if (prec_num >= hl->nmark)
        hseq = 0;
    else if (prec_num > 0)
        hseq = hl->nmark - prec_num;
    else
        hseq = hl->nmark - 1;
    do {
        if (hseq < 0)
            return;
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
        if (an == NULL)
            an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
        hseq--;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the nth anchor */
void nthA(struct CmdArgs args)
{
    struct HmarkerList* hl = Currentbuf->hmarklist;
    struct BufferPoint* po;
    struct Anchor* an;

    int n = searchKeyNum();
    if (n < 0 || n > hl->nmark)
        return;

    if (Currentbuf->firstLine == NULL)
        return;
    if (!hl || hl->nmark == 0)
        return;

    po = hl->marks + n - 1;
    an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
    if (an == NULL)
        an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
    if (an == NULL)
        return;

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next anchor */
void nextA(struct CmdArgs args)
{
    _nextA(FALSE);
}

/* go to the previous anchor */
void prevA(struct CmdArgs args)
{
    _prevA(FALSE);
}

/* go to the next visited anchor */
void nextVA(struct CmdArgs args)
{
    _nextA(TRUE);
}

/* go to the previous visited anchor */
void prevVA(struct CmdArgs args)
{
    _prevA(TRUE);
}

/* go to the next left anchor */
void nextL(struct CmdArgs args)
{
    nextX(-1, 0);
}

/* go to the next left-up anchor */
void nextLU(struct CmdArgs args)
{
    nextX(-1, -1);
}

/* go to the next right anchor */
void nextR(struct CmdArgs args)
{
    nextX(1, 0);
}

/* go to the next right-down anchor */
void nextRD(struct CmdArgs args)
{
    nextX(1, 1);
}

/* go to the next downward anchor */
void nextD(struct CmdArgs args)
{
    nextY(1);
}

/* go to the next upward anchor */
void nextU(struct CmdArgs args)
{
    nextY(-1);
}

/* go to the next bufferr */
void nextBf(struct CmdArgs args)
{
    Buffer* buf;
    int i;

    for (i = 0; i < PREC_NUM; i++) {
        buf = prevBuffer(Firstbuf, Currentbuf);
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* go to the previous bufferr */
void prevBf(struct CmdArgs args)
{
    Buffer* buf;
    int i;

    for (i = 0; i < PREC_NUM; i++) {
        buf = Currentbuf->nextBuffer;
        if (!buf) {
            if (i == 0)
                return;
            break;
        }
        Currentbuf = buf;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* delete current buffer and back to the previous buffer */
void backBf(struct CmdArgs args)
{
    Buffer* buf = Currentbuf->linkBuffer[LB_N_FRAME];

    if (!checkBackBuffer(Currentbuf)) {
        if (close_tab_back && nTab >= 1) {
            deleteTab(CurrentTab);
            displayBuffer(Currentbuf, B_FORCE_REDRAW);
        } else
            /* FIXME: gettextize? */
            disp_message("Can't go back...", TRUE);
        return;
    }

    delBuffer(Currentbuf);

    if (buf) {
        if (buf->frameQ) {
            struct frameset* fs;
            long linenumber = buf->frameQ->linenumber;
            long top = buf->frameQ->top_linenumber;
            int pos = buf->frameQ->pos;
            int currentColumn = buf->frameQ->currentColumn;
            struct AnchorList* formitem = buf->frameQ->formitem;

            fs = popFrameTree(&(buf->frameQ));
            deleteFrameSet(buf->frameset);
            buf->frameset = fs;

            if (buf == Currentbuf) {
                rFrame((struct CmdArgs) { 0 });
                Currentbuf->topLine = lineSkip(Currentbuf,
                    Currentbuf->firstLine, top - 1,
                    FALSE);
                gotoLine(Currentbuf, linenumber);
                Currentbuf->pos = pos;
                Currentbuf->currentColumn = currentColumn;
                arrangeCursor(Currentbuf);
                formResetBuffer(Currentbuf, formitem);
            }
        } else if (RenderFrame && buf == Currentbuf) {
            delBuffer(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void deletePrevBuf(struct CmdArgs args)
{
    Buffer* buf = Currentbuf->nextBuffer;
    if (buf)
        delBuffer(buf);
}

void goURL(struct CmdArgs args)
{
    goURL0("Goto URL: ", FALSE);
}

void goHome(struct CmdArgs args)
{
    char* url;
    if ((url = getenv("HTTP_HOME")) != NULL || (url = getenv("WWW_HOME")) != NULL) {
        struct Url p_url;
        Buffer* cur_buf = Currentbuf;
        SKIP_BLANKS(url);
        url = url_encode(url, NULL, 0);
        parseURL2(url, &p_url, NULL);
        pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
        cmd_loadURL(url, NULL, NULL, NULL);
        if (Currentbuf != cur_buf) /* success */
            pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
    }
}

void gorURL(struct CmdArgs args)
{
    goURL0("Goto relative URL: ", TRUE);
}

/* load bookmark */
void ldBmark(struct CmdArgs args)
{
    cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}

/* Add current to bookmark */
void adBmark(struct CmdArgs args)
{
    Str tmp;
    FormList* request;

    tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                  "&charset=%s",
        (Str_form_quote(localCookie()))->ptr,
        (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
        (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->ptr,
        (Str_form_quote(Strnew_wc_output(wc_conv_strict(WcOption, Currentbuf->buffername, InnerCharset, BookmarkCharset))))->ptr,
        wc_ces_to_charset(BookmarkCharset));
    request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
    request->body = tmp->ptr;
    request->length = tmp->length;
    cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER,
        request);
}

/* option setting */
void ldOpt(struct CmdArgs args)
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
void setOpt(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
        if (opt != NULL && *opt != '\0') {
            char* v = get_param_option(opt);
            opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
        }
        opt = inputStrHist("Set option: ", opt, TextHist);
        if (opt == NULL || *opt == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    if (set_param_option(opt))
        sync_with_option();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

/* error message list */
void msgs(struct CmdArgs args)
{
    cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
void pginfo(struct CmdArgs args)
{
    Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != NULL) {
        Currentbuf = buf;
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if ((buf = Currentbuf->linkBuffer[LB_INFO]) != NULL)
        delBuffer(buf);
    buf = page_info_panel(Currentbuf);
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

/* link menu */
void linkMn(struct CmdArgs args)
{
    struct LinkList* l = link_menu(Currentbuf);
    struct Url p_url;

    if (!l || !l->url)
        return;
    if (*(l->url) == '#') {
        gotoLabel(l->url + 1);
        return;
    }
    parseURL2(l->url, &p_url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(l->url, baseURL(Currentbuf),
        parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
}

/* accesskey */
void accessKey(struct CmdArgs args)
{
    anchorMn(accesskey_menu, TRUE);
}

/* list menu */
void listMn(struct CmdArgs args)
{
    anchorMn(list_menu, TRUE);
}

void movlistMn(struct CmdArgs args)
{
    anchorMn(list_menu, FALSE);
}

/* link,anchor,image list */
void linkLst(struct CmdArgs args)
{
    Buffer* buf;

    buf = link_list_panel(Currentbuf);
    if (buf != NULL) {
        buf->document_charset = Currentbuf->document_charset;
        cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
    }
}

/* cookie list */
void cooLst(struct CmdArgs args)
{
    Buffer* buf;

    buf = cookie_list_panel();
    if (buf != NULL)
        cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
void ldHist(struct CmdArgs args)
{
    Str html = historyBuffer(URLHist);
    cmd_loadBuffer(loadHTMLString(html), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
void svA(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    do_download = TRUE;
    followA((struct CmdArgs) { 0 });
    do_download = FALSE;
}

/* download IMG link */
void svI(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    do_download = TRUE;
    followI((struct CmdArgs) { 0 });
    do_download = FALSE;
}

/* save buffer */
void svBuf(struct CmdArgs args)
{
    const char *qfile = NULL, *file;
    FILE* f;
    int is_pipe;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    file = searchKeyData();
    if (file == NULL || *file == '\0') {
        /* FIXME: gettextize? */
        qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, SaveHist);
        if (qfile == NULL || *qfile == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    file = conv_to_system(qfile ? qfile : file);
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
            displayBuffer(Currentbuf, B_NORMAL);
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
    displayBuffer(Currentbuf, B_NORMAL);
}

/* save source */
void svSrc(struct CmdArgs args)
{
    if (Currentbuf->sourcefile == NULL)
        return;
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    PermitSaveToPipe = TRUE;
    const char* file;
    if (Currentbuf->real_scheme == SCM_LOCAL)
        file = conv_from_system(guess_save_name(NULL,
            Currentbuf->currentURL.real_file));
    else
        file = guess_save_name(Currentbuf, Currentbuf->currentURL.file);
    doFileCopy(Currentbuf->sourcefile, file);
    PermitSaveToPipe = FALSE;
    displayBuffer(Currentbuf, B_NORMAL);
}

/* peek URL */
void peekURL(struct CmdArgs args)
{
    _peekURL(0);
}

/* peek URL of image */
void peekIMG(struct CmdArgs args)
{
    _peekURL(1);
}

void curURL(struct CmdArgs args)
{
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;
    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (CurrentKey == prev_key && s != NULL) {
        if (s->length - offset >= COLS)
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
    } else {
        offset = 0;
        s = currentURL();
        if (DecodeURL)
            s = Strnew_charp(url_decode2(s->ptr, NULL));
        s = checkType(s, &pp, NULL);
        p = NewAtom_N(Lineprop, s->length);
        bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
        offset = (n - 1) * (COLS - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message_nomouse(&s->ptr[offset], TRUE);
}
/* view HTML source */

void vwSrc(struct CmdArgs args)
{
    Buffer* buf;

    if (Currentbuf->type == NULL || Currentbuf->bufferprop & BP_FRAME)
        return;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL || (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
        Currentbuf = buf;
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if (Currentbuf->sourcefile == NULL) {
        if (Currentbuf->pagerSource && !strcasecmp(Currentbuf->type, "text/plain")) {
            wc_ces old_charset;
            wc_bool old_fix_width_conv;
            FILE* f;
            Str tmpf = tmpfname(TMPF_SRC, NULL);
            f = fopen(tmpf->ptr, "w");
            if (f == NULL)
                return;
            old_charset = DisplayCharset;
            old_fix_width_conv = WcOption.fix_width_conv;
            DisplayCharset = (Currentbuf->document_charset != WC_CES_US_ASCII)
                ? Currentbuf->document_charset
                : 0;
            WcOption.fix_width_conv = WC_FALSE;
            saveBufferBody(Currentbuf, f, TRUE);
            DisplayCharset = old_charset;
            WcOption.fix_width_conv = old_fix_width_conv;
            fclose(f);
            Currentbuf->sourcefile = tmpf->ptr;
        } else {
            return;
        }
    }

    buf = newBuffer(INIT_BUFFER_WIDTH);

    if (is_html_type(Currentbuf->type)) {
        buf->type = "text/plain";
        if (Currentbuf->real_type && is_html_type(Currentbuf->real_type))
            buf->real_type = "text/plain";
        else
            buf->real_type = Currentbuf->real_type;
        buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
        buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_SOURCE] = buf;
    } else if (!strcasecmp(Currentbuf->type, "text/plain")) {
        buf->type = "text/html";
        if (Currentbuf->real_type && !strcasecmp(Currentbuf->real_type, "text/plain"))
            buf->real_type = "text/html";
        else
            buf->real_type = Currentbuf->real_type;
        buf->buffername = Sprintf("HTML view of %s",
            Currentbuf->buffername)
                              ->ptr;
        buf->linkBuffer[LB_SOURCE] = Currentbuf;
        Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
    } else {
        return;
    }
    buf->currentURL = Currentbuf->currentURL;
    buf->real_scheme = Currentbuf->real_scheme;
    buf->filename = Currentbuf->filename;
    buf->sourcefile = Currentbuf->sourcefile;
    buf->header_source = Currentbuf->header_source;
    buf->search_header = Currentbuf->search_header;
    buf->document_charset = Currentbuf->document_charset;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;

    buf->need_reshape = TRUE;
    reshapeBuffer(buf);
    pushBuffer(buf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
void reload(struct CmdArgs args)
{
    Buffer *buf, *fbuf = NULL, sbuf;
    wc_ces old_charset;
    Str url;
    FormList* request;
    int multipart;

    if (Currentbuf->bufferprop & BP_INTERNAL) {
        if (0 == strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE)) {
            ldDL((struct CmdArgs) { 0 });
            return;
        }
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL && !strcmp(Currentbuf->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't reload stdin", TRUE);
        return;
    }
    copyBuffer(&sbuf, Currentbuf);
    if (Currentbuf->bufferprop & BP_FRAME && (fbuf = Currentbuf->linkBuffer[LB_N_FRAME])) {
        if (fmInitialized) {
            message("Rendering frame", 0, 0);
            refresh();
        }
        if (!(buf = renderFrame(fbuf, 1))) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
        if (fbuf->linkBuffer[LB_FRAME]) {
            if (buf->sourcefile && fbuf->linkBuffer[LB_FRAME]->sourcefile && !strcmp(buf->sourcefile, fbuf->linkBuffer[LB_FRAME]->sourcefile))
                fbuf->linkBuffer[LB_FRAME]->sourcefile = NULL;
            delBuffer(fbuf->linkBuffer[LB_FRAME]);
        }
        fbuf->linkBuffer[LB_FRAME] = buf;
        buf->linkBuffer[LB_N_FRAME] = fbuf;
        pushBuffer(buf);
        Currentbuf = buf;
        if (Currentbuf->firstLine) {
            COPY_BUFROOT(Currentbuf, &sbuf);
            restorePosition(Currentbuf, &sbuf);
        }
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
        return;
    } else if (Currentbuf->frameset != NULL)
        fbuf = Currentbuf->linkBuffer[LB_FRAME];
    multipart = 0;
    if (Currentbuf->form_submit) {
        request = Currentbuf->form_submit->parent;
        if (request->method == FORM_METHOD_POST
            && request->enctype == FORM_ENCTYPE_MULTIPART) {
            Str query;
            struct stat st;
            multipart = 1;
            query_from_followform(&query, Currentbuf->form_submit, multipart);
            stat(request->body, &st);
            request->length = st.st_size;
        }
    } else {
        request = NULL;
    }
    url = parsedURL2Str(&Currentbuf->currentURL);
    /* FIXME: gettextize? */
    message("Reloading...", 0, 0);
    refresh();
    old_charset = DocumentCharset;
    if (Currentbuf->document_charset != WC_CES_US_ASCII)
        DocumentCharset = Currentbuf->document_charset;
    SearchHeader = Currentbuf->search_header;
    DefaultType = Currentbuf->real_type;
    buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
    DocumentCharset = old_charset;
    SearchHeader = FALSE;
    DefaultType = NULL;

    if (multipart)
        unlink(request->body);
    if (buf == NULL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't reload...", TRUE);
        return;
    } else if (buf == NO_BUFFER) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if (fbuf != NULL)
        Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(Currentbuf, buf);
    if ((buf->type != NULL) && (sbuf.type != NULL) && ((!strcasecmp(buf->type, "text/plain") && is_html_type(sbuf.type)) || (is_html_type(buf->type) && !strcasecmp(sbuf.type, "text/plain")))) {
        vwSrc((struct CmdArgs) { 0 });
        if (Currentbuf != buf)
            Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    Currentbuf->search_header = sbuf.search_header;
    Currentbuf->form_submit = sbuf.form_submit;
    if (Currentbuf->firstLine) {
        COPY_BUFROOT(Currentbuf, &sbuf);
        restorePosition(Currentbuf, &sbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* reshape */
void reshape(struct CmdArgs args)
{
    Currentbuf->need_reshape = TRUE;
    reshapeBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void docCSet(struct CmdArgs args)
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Document charset: ",
            wc_ces_to_charset(Currentbuf->document_charset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset == 0) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    _docCSet(charset);
}

void defCSet(struct CmdArgs args)
{
    const char* cs = searchKeyData();
    if (cs == NULL || *cs == '\0')
        /* FIXME: gettextize? */
        cs = inputStr("Default document charset: ",
            wc_ces_to_charset(DocumentCharset));
    wc_ces charset = wc_guess_charset_short(cs, 0);
    if (charset != 0)
        DocumentCharset = charset;
    displayBuffer(Currentbuf, B_NORMAL);
}

void chkURL(struct CmdArgs args)
{
    chkURLBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void chkWORD(struct CmdArgs args)
{
    char* p;
    int spos, epos;
    p = getCurWord(Currentbuf, &spos, &epos);
    if (p == NULL)
        return;
    reAnchorWord(Currentbuf, Currentbuf->currentLine, spos, epos);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void chkNMID(struct CmdArgs args)
{
    chkNMIDBuffer(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* render frames */
void rFrame(struct CmdArgs args)
{
    Buffer* buf;

    if ((buf = Currentbuf->linkBuffer[LB_FRAME]) != NULL) {
        Currentbuf = buf;
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    if (Currentbuf->frameset == NULL) {
        if ((buf = Currentbuf->linkBuffer[LB_N_FRAME]) != NULL) {
            Currentbuf = buf;
            displayBuffer(Currentbuf, B_NORMAL);
        }
        return;
    }
    if (fmInitialized) {
        message("Rendering frame", 0, 0);
        refresh();
    }
    buf = renderFrame(Currentbuf, 0);
    if (buf == NULL) {
        displayBuffer(Currentbuf, B_NORMAL);
        return;
    }
    buf->linkBuffer[LB_N_FRAME] = Currentbuf;
    Currentbuf->linkBuffer[LB_FRAME] = buf;
    pushBuffer(buf);
    if (fmInitialized && display_ok)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void extbrz(struct CmdArgs args)
{
    if (Currentbuf->bufferprop & BP_INTERNAL) {
        /* FIXME: gettextize? */
        disp_err_message("Can't browse...", TRUE);
        return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL && !strcmp(Currentbuf->currentURL.file, "-")) {
        /* file is std input */
        /* FIXME: gettextize? */
        disp_err_message("Can't browse stdin", TRUE);
        return;
    }
    invoke_browser(parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

void linkbrz(struct CmdArgs args)
{
    struct Anchor* a;
    struct Url pu;

    if (Currentbuf->firstLine == NULL)
        return;
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
        return;
    parseURL2(a->url, &pu, baseURL(Currentbuf));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
void curlno(struct CmdArgs args)
{
    struct Line* l = Currentbuf->currentLine;
    Str tmp;
    int cur = 0, all = 0, col = 0, len = 0;

    if (l != NULL) {
        cur = l->real_linenumber;
        col = l->bwidth + Currentbuf->currentColumn + Currentbuf->cursorX + 1;
        while (l->next && l->next->bpos)
            l = l->next;
        if (l->width < 0)
            l->width = COLPOS(l, l->len);
        len = l->bwidth + l->width;
    }
    if (Currentbuf->lastLine)
        all = Currentbuf->lastLine->real_linenumber;
    if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
        tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
        tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
            (int)((double)cur * 100.0 / (double)(all ? all : 1)
                + 0.5),
            col, len);
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->document_charset));

    disp_message(tmp->ptr, FALSE);
}

void dispI(struct CmdArgs args)
{
    if (!displayImage)
        initImage();
    if (!activeImage)
        return;
    displayImage = TRUE;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->image_flag = IMG_FLAG_AUTO;
    Currentbuf->need_reshape = TRUE;
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void stopI(struct CmdArgs args)
{
    if (!activeImage)
        return;
    /*
     * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
     * return;
     */
    Currentbuf->image_flag = IMG_FLAG_SKIP;
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void dispVer(struct CmdArgs args)
{
    disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

void wrapToggle(struct CmdArgs args)
{
    if (WrapSearch) {
        WrapSearch = FALSE;
        /* FIXME: gettextize? */
        disp_message("Wrap search off", TRUE);
    } else {
        WrapSearch = TRUE;
        /* FIXME: gettextize? */
        disp_message("Wrap search on", TRUE);
    }
}

void dictword(struct CmdArgs args)
{
    execdict(inputStr("(dictionary)!", ""));
}

void dictwordat(struct CmdArgs args)
{
    execdict(GetWord(Currentbuf));
}

void execCmd(struct CmdArgs args)
{
    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("command [; ...]: ", "", TextHist);
        if (data == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
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
        const char* cmd = getWord(&data);
        if (!cmd)
            break;
        const char* p = getQWord(&data);
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = *p ? p : NULL;
        w3mFunc(cmd);
        CurrentCmdData = NULL;
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void setAlarm(struct CmdArgs args)
{
    int sec = 0;
    const char* cmd = 0;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    const char* data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("(Alarm)sec command: ", "", TextHist);
        if (data == NULL) {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    if (*data != '\0') {
        sec = atoi(getWord(&data));
        if (sec > 0)
            cmd = getWord(&data);
    }
    if (cmd >= 0) {
        data = getQWord(&data);
        setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, data);
        disp_message_nsec(Sprintf("%dsec %s %s", sec, cmd,
                              data)
                              ->ptr,
            FALSE, 1, FALSE, TRUE);
    } else {
        setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, "NOTHING", NULL);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void reinit(struct CmdArgs args)
{
    char* resource = searchKeyData();

    if (resource == NULL) {
        init_rc();
        sync_with_option();
        initCookie();
        displayBuffer(Currentbuf, B_REDRAW_IMAGE);
        return;
    }

    if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
        init_rc();
        sync_with_option();
        displayBuffer(Currentbuf, B_REDRAW_IMAGE);
        return;
    }

    if (!strcasecmp(resource, "COOKIE")) {
        initCookie();
        return;
    }

    if (!strcasecmp(resource, "KEYMAP")) {
        initKeymap(TRUE);
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

    disp_err_message(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, FALSE);
}

void defKey(struct CmdArgs args)
{
    char* data;

    CurrentKeyData = NULL; /* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0') {
        data = inputStrHist("Key definition: ", "", TextHist);
        if (data == NULL || *data == '\0') {
            displayBuffer(Currentbuf, B_NORMAL);
            return;
        }
    }
    setKeymap(allocStr(data, -1), -1, TRUE);
    displayBuffer(Currentbuf, B_NORMAL);
}

void newT(struct CmdArgs args)
{
    _newT();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void closeT(struct CmdArgs args)
{
    TabBuffer* tab;

    if (nTab <= 1)
        return;
    if (prec_num)
        tab = numTab(PREC_NUM);
    else
        tab = CurrentTab;
    if (tab)
        deleteTab(tab);
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void nextT(struct CmdArgs args)
{
    int i;

    if (nTab <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab->nextTab)
            CurrentTab = CurrentTab->nextTab;
        else
            CurrentTab = FirstTab;
    }
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void prevT(struct CmdArgs args)
{
    int i;

    if (nTab <= 1)
        return;
    for (i = 0; i < PREC_NUM; i++) {
        if (CurrentTab->prevTab)
            CurrentTab = CurrentTab->prevTab;
        else
            CurrentTab = LastTab;
    }
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

void tabA(struct CmdArgs args)
{
    followTab(prec_num ? numTab(PREC_NUM) : NULL);
}

void tabURL(struct CmdArgs args)
{
    tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
        "Goto URL on new tab: ", FALSE);
}

void tabrURL(struct CmdArgs args)
{
    tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
        "Goto relative URL on new tab: ", TRUE);
}

void tabR(struct CmdArgs args)
{
    TabBuffer* tab;
    int i;

    for (tab = CurrentTab, i = 0; tab && i < PREC_NUM;
        tab = tab->nextTab, i++)
        ;
    moveTab(CurrentTab, tab ? tab : LastTab, TRUE);
}

void tabL(struct CmdArgs args)
{
    TabBuffer* tab;
    int i;

    for (tab = CurrentTab, i = 0; tab && i < PREC_NUM;
        tab = tab->prevTab, i++)
        ;
    moveTab(CurrentTab, tab ? tab : FirstTab, FALSE);
}

void ldDL(struct CmdArgs args)
{
    downloadListPanel();
}

void undoPos(struct CmdArgs args)
{
    BufferPos* b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
        return;
    if (!b || !b->prev)
        return;
    for (i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
        ;
    resetPos(b);
}

void redoPos(struct CmdArgs args)
{
    BufferPos* b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
        return;
    if (!b || !b->next)
        return;
    for (i = 0; i < PREC_NUM && b->next; i++, b = b->next)
        ;
    resetPos(b);
}

void cursorTop(struct CmdArgs args)
{
    if (Currentbuf->firstLine == NULL)
        return;
    Currentbuf->currentLine = lineSkip(Currentbuf, Currentbuf->topLine,
        0, FALSE);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

void cursorMiddle(struct CmdArgs args)
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = (Currentbuf->LINES - 1) / 2;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, FALSE);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

void cursorBottom(struct CmdArgs args)
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
        return;
    offsety = Currentbuf->LINES - 1;
    Currentbuf->currentLine = currentLineSkip(Currentbuf, Currentbuf->topLine,
        offsety, FALSE);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

void mainMn(struct CmdArgs args)
{
    Menu* menu = &MainMenu;
    char* data;
    int n;
    int x = Currentbuf->cursorX + Currentbuf->rootX,
        y = Currentbuf->cursorY + Currentbuf->rootY;

    data = searchKeyData();
    if (data != NULL) {
        n = getMenuN(w3mMenuList, data);
        if (n < 0)
            return;
        menu = w3mMenuList[n].menu;
    }
    popupMenu(x, y, menu);
}

void selMn(struct CmdArgs args)
{
    int x = Currentbuf->cursorX + Currentbuf->rootX,
        y = Currentbuf->cursorY + Currentbuf->rootY;

    popupMenu(x, y, &SelectMenu);
}

void tabMn(struct CmdArgs args)
{
    int x = Currentbuf->cursorX + Currentbuf->rootX,
        y = Currentbuf->cursorY + Currentbuf->rootY;

    popupMenu(x, y, &SelTabMenu);
}
