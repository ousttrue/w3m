#include "defun.h"
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
// #include "file.h"
#include <stdlib.h>
#include <string.h>

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing")
{
}

DEFUN(rdrwSc, REDRAW, "Draw the screen anew")
{
    tty_clear();
    screen_clear();
    doc_arrangeCursor(&ctx.buf->doc);
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
    // saveBuffer(Currentbuf, f, TRUE);
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
        if (buf->content.content_type == NULL)
            buf->content.content_type = "text/plain";
        tab_push_buffer(getRuntime()->CurrentTab, buf);
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

    struct Content content = get_content_cache(file_to_url(fn), NULL,
        (struct LoadOption) {
            .base_url = NULL,
            .referer = NO_REFERER,
            .flag = 0,
        });
    if (content.content_type == NULL) {
        char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
        disp_err_message(emsg, FALSE);
        return;
    }
    struct Buffer* buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->content = content;
    tab_push_buffer(getRuntime()->CurrentTab, buf);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel")
{
    const char* lang = getRuntime()->AcceptLang;
    int n = strcspn(lang, ";, \t");
    Str tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
        Str_form_quote(Strnew_charp(w3m_version))->ptr,
        Str_form_quote(Strnew_charp_n(lang, n))->ptr);
    struct Content content = get_content_cache(tmp->ptr, NULL,
        (struct LoadOption) {
            .base_url = NULL,
            .referer = NO_REFERER,
            0,
        });
    if (!content.content_type) {
        Str emsg = Sprintf("Can't load %s", conv_from_system(tmp->ptr));
        disp_err_message(emsg->ptr, false);
        return;
    }
    struct Buffer* buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->content = content;
    tab_push_buffer(getRuntime()->CurrentTab, buf);
}
