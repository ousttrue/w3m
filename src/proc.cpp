#include "proto.h"
#include "http_response.h"
#include "url_quote.h"
#include "search.h"
#include "bufferpos.h"
#include "mimetypes.h"
#include "funcname1.h"
#include "alarm.h"
#include "downloadlist.h"
#include "url_stream.h"
#include "linein.h"
#include "cookie.h"
#include "rc.h"
#include "form.h"
#include "history.h"
#include "anchor.h"
#include "tmpfile.h"
#include "util.h"
#include "etc.h"
#include "mailcap.h"
#include "quote.h"
#include "http_request.h"
#include "http_response.h"
#include "message.h"
#include "http_session.h"
#include "signal_util.h"
#include "local_cgi.h"
#include "Str.h"
#include "search.h"
#include "display.h"
#include "w3m.h"
#include "app.h"
#include "terms.h"
#include "screen.h"
#include "myctype.h"
#include "func.h"
#include "tabbuffer.h"
#include "buffer.h"
#include <sys/stat.h>

#define DEFUN(funcname, macroname, docstring) void funcname(void)

DEFUN(nulcmd, NOTHING nullptr @ @ @, "Do nothing") { /* do nothing */
}

DEFUN(escmap, ESCMAP, "ESC map") {
  char c = getch();
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map") {
  char c = getch();
  if (IS_DIGIT(c)) {
    escdmap(c);
    return;
  }
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESCB, EscBKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap") {
  char c;
  c = getch();
  if (IS_ASCII(c)) {
    CurrentKey = K_MULTI | (CurrentKey << 16) | c;
    escKeyProc((int)c, 0, nullptr);
  }
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page") {
  if (vi_prec_num)
    nscroll(searchKeyNum() * (Currentbuf->layout.LINES - 1), B_NORMAL);
  else
    nscroll(prec_num ? searchKeyNum()
                     : searchKeyNum() * (Currentbuf->layout.LINES - 1),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page") {
  if (vi_prec_num)
    nscroll(-searchKeyNum() * (Currentbuf->layout.LINES - 1), B_NORMAL);
  else
    nscroll(-(prec_num ? searchKeyNum()
                       : searchKeyNum() * (Currentbuf->layout.LINES - 1)),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page") {
  nscroll(searchKeyNum() * (Currentbuf->layout.LINES / 2 - 1), B_NORMAL);
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page") {
  nscroll(-searchKeyNum() * (Currentbuf->layout.LINES / 2 - 1), B_NORMAL);
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line") {
  nscroll(searchKeyNum(), B_SCROLL);
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line") {
  nscroll(-searchKeyNum(), B_SCROLL);
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line") {
  int offsety;
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  offsety = Currentbuf->layout.LINES / 2 - Currentbuf->layout.cursorY;
  if (offsety != 0) {
    Currentbuf->layout.topLine = Currentbuf->layout.lineSkip(
        Currentbuf->layout.topLine, -offsety, false);
    Currentbuf->layout.arrangeLine();
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column") {
  int offsetx;
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  offsetx = Currentbuf->layout.cursorX - Currentbuf->layout.COLS / 2;
  if (offsetx != 0) {
    Currentbuf->layout.columnSkip(offsetx);
    Currentbuf->layout.arrangeCursor();
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew") {
  clear();
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward") {
  srch(forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward") {
  isrch(forwardSearch, "I-search: ");
}

/* Search regular expression backward */

DEFUN(srchbak, SEARCH_BACK, "Search backward") {
  srch(backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward") {
  isrch(backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward") { srch_nxtprv(0); }

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward") { srch_nxtprv(1); }

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left") {
  int column;

  if (Currentbuf->layout.firstLine == nullptr)
    return;
  column = Currentbuf->layout.currentColumn;
  Currentbuf->layout.columnSkip(
      searchKeyNum() * (-Currentbuf->layout.COLS + 1) + 1);
  shiftvisualpos(Currentbuf, Currentbuf->layout.currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right") {
  int column;

  if (Currentbuf->layout.firstLine == nullptr)
    return;
  column = Currentbuf->layout.currentColumn;
  Currentbuf->layout.columnSkip(searchKeyNum() * (Currentbuf->layout.COLS - 1) -
                                1);
  shiftvisualpos(Currentbuf, Currentbuf->layout.currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1R, RIGHT, "Shift screen one column right") {
  Buffer *buf = Currentbuf;
  Line *l = buf->layout.currentLine;
  int j, column, n = searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    column = buf->layout.currentColumn;
    Currentbuf->layout.columnSkip(1);
    if (column == buf->layout.currentColumn)
      break;
    shiftvisualpos(Currentbuf, 1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1L, LEFT, "Shift screen one column left") {
  Buffer *buf = Currentbuf;
  Line *l = buf->layout.currentLine;
  int j, n = searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    if (buf->layout.currentColumn == 0)
      break;
    Currentbuf->layout.columnSkip(-1);
    shiftvisualpos(Currentbuf, -1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setEnv, SETENV, "Set environment variable") {
  const char *env;
  const char *var, *value;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  env = searchKeyData();
  if (env == nullptr || *env == '\0' || strchr(env, '=') == nullptr) {
    if (env != nullptr && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    // env = inputStrHist("Set environ: ", env, TextHist);
    if (env == nullptr || *env == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if ((value = strchr(env, '=')) != nullptr && value > env) {
    var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(pipeBuf, PIPE_BUF,
      "Pipe current buffer through a shell command and display output") {}

/* Execute shell command and read output ac pipe. */
DEFUN(pipesh, PIPE_SHELL, "Execute shell command and display output") {}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output") {
  Buffer *buf;
  const char *cmd;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  cmd = searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd == nullptr || *cmd == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  // MySignalHandler prevtrap = {};
  // prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  buf = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  term_raw();
  if (buf == nullptr) {
    disp_message("Execution failed", true);
    return;
  } else {
    if (buf->info->type == nullptr)
      buf->info->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output") {
  const char *cmd;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  cmd = searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != nullptr && *cmd != '\0') {
    fmTerm();
    printf("\n");
    (void)!system(cmd); /* We do not care about the exit code here! */
    printf("\n[Hit any key]");
    fflush(stdout);
    fmInit();
    getch();
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer") {
  auto fn = searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn == nullptr || *fn == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  cmd_loadfile(fn);
}

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel") {
  auto lang = AcceptLang;
  auto n = strcspn(lang, ";, \t");
  auto tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  cmd_loadURL(tmp->ptr, {}, NO_REFERER, nullptr);
}

DEFUN(movL, MOVE_LEFT, "Cursor left") { _movL(Currentbuf->layout.COLS / 2); }

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide") { _movL(1); }

DEFUN(movD, MOVE_DOWN, "Cursor down") {
  _movD((Currentbuf->layout.LINES + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide") { _movD(1); }

DEFUN(movU, MOVE_UP, "Cursor up") { _movU((Currentbuf->layout.LINES + 1) / 2); }

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide") { _movU(1); }

DEFUN(movR, MOVE_RIGHT, "Cursor right") { _movR(Currentbuf->layout.COLS / 2); }

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide") {
  _movR(1);
}

DEFUN(movLW, PREV_WORD, "Move to the previous word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->layout.firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->layout.currentLine;
    ppos = Currentbuf->layout.pos;

    if (prev_nonnull_line(Currentbuf->layout.currentLine) < 0)
      goto end;

    while (1) {
      l = Currentbuf->layout.currentLine;
      lb = l->lineBuf.data();
      while (Currentbuf->layout.pos > 0) {
        int tmp = Currentbuf->layout.pos;
        prevChar(tmp, l);
        if (is_wordchar(lb[tmp])) {
          break;
        }
        Currentbuf->layout.pos = tmp;
      }
      if (Currentbuf->layout.pos > 0)
        break;
      if (prev_nonnull_line(Currentbuf->layout.currentLine->prev) < 0) {
        Currentbuf->layout.currentLine = pline;
        Currentbuf->layout.pos = ppos;
        goto end;
      }
      Currentbuf->layout.pos = Currentbuf->layout.currentLine->len;
    }

    l = Currentbuf->layout.currentLine;
    lb = l->lineBuf.data();
    while (Currentbuf->layout.pos > 0) {
      int tmp = Currentbuf->layout.pos;
      prevChar(tmp, l);
      if (!is_wordchar(lb[tmp])) {
        break;
      }
      Currentbuf->layout.pos = tmp;
    }
  }
end:
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movRW, NEXT_WORD, "Move to the next word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->layout.firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->layout.currentLine;
    ppos = Currentbuf->layout.pos;

    if (next_nonnull_line(Currentbuf->layout.currentLine) < 0)
      goto end;

    l = Currentbuf->layout.currentLine;
    lb = l->lineBuf.data();
    while (Currentbuf->layout.pos < l->len &&
           is_wordchar(lb[Currentbuf->layout.pos])) {
      nextChar(Currentbuf->layout.pos, l);
    }

    while (1) {
      while (Currentbuf->layout.pos < l->len &&
             !is_wordchar(lb[Currentbuf->layout.pos])) {
        nextChar(Currentbuf->layout.pos, l);
      }
      if (Currentbuf->layout.pos < l->len)
        break;
      if (next_nonnull_line(Currentbuf->layout.currentLine->next) < 0) {
        Currentbuf->layout.currentLine = pline;
        Currentbuf->layout.pos = ppos;
        goto end;
      }
      Currentbuf->layout.pos = 0;
      l = Currentbuf->layout.currentLine;
      lb = l->lineBuf.data();
    }
  }
end:
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit without confirmation") { _quitfm(false); }

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request") {
  _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel") {
  Buffer *buf;
  int ok;
  char cmd;

  ok = false;
  do {
    buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
    switch (cmd) {
    case 'B':
      ok = true;
      break;
    case '\n':
    case ' ':
      Currentbuf = buf;
      ok = true;
      break;
    case 'D':
      CurrentTab->deleteBuffer(buf);
      if (Firstbuf == nullptr) {
        /* No more buffer */
        Firstbuf = nullBuffer();
        Currentbuf = Firstbuf;
      }
      break;
    case 'q':
      qquitfm();
      break;
    case 'Q':
      quitfm();
      break;
    }
  } while (!ok);

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background") {
#ifndef SIGSTOP
  char *shell;
#endif /* not SIGSTOP */
  move(LASTLINE, 0);
  clrtoeolx();
  refresh(term_io());
  fmTerm();
#ifndef SIGSTOP
  shell = getenv("SHELL");
  if (shell == nullptr)
    shell = "/bin/sh";
  system(shell);
#else  /* SIGSTOP */
  signal(SIGTSTP, SIG_DFL); /* just in case */
  /*
   * Note: If susp() was called from SIGTSTP handler,
   * unblocking SIGTSTP would be required here.
   * Currently not.
   */
  kill(0, SIGTSTP); /* stop whole job, not a single process */
#endif /* SIGSTOP */
  fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line") {

  auto str = searchKeyData();
  if (prec_num)
    _goLine("^");
  else if (str)
    _goLine(str);
  else {
    // _goLine(inputStr("Goto line: ", ""));
  }
}

DEFUN(goLineF, BEGIN, "Go to the first line") { _goLine("^"); }

DEFUN(goLineL, END, "Go to the last line") { _goLine("$"); }

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line") {
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  while (Currentbuf->layout.currentLine->prev &&
         Currentbuf->layout.currentLine->bpos) {
    Currentbuf->layout.cursorUp0(1);
  }
  Currentbuf->layout.pos = 0;
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line") {
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  while (Currentbuf->layout.currentLine->next &&
         Currentbuf->layout.currentLine->next->bpos) {
    Currentbuf->layout.cursorDown0(1);
  }
  Currentbuf->layout.pos = Currentbuf->layout.currentLine->len - 1;
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source") {
  auto fn = Currentbuf->info->filename;

  if (fn == nullptr || /* Behaving as a pager */
      (Currentbuf->info->type == nullptr &&
       Currentbuf->info->edit == nullptr) || /* Reading shell */
      Currentbuf->info->currentURL.schema != SCM_LOCAL ||
      Currentbuf->info->currentURL.file == "-" /* file is std input  */
  ) {
    disp_err_message("Can't edit other than local file", true);
    return;
  }

  Str *cmd;
  if (Currentbuf->info->edit) {
    cmd =
        unquote_mailcap(Currentbuf->info->edit, Currentbuf->info->real_type, fn,
                        Currentbuf->info->getHeader("Content-Type:"), nullptr);
  } else {
    cmd = myEditor(Editor, shell_quote(fn), cur_real_linenumber(Currentbuf));
  }
  exec_cmd(cmd->ptr);

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document") {
  auto tmpf = tmpfname(TMPF_DFL, nullptr)->ptr;
  auto f = fopen(tmpf, "w");
  if (f == nullptr) {
    disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, true);
    return;
  }
  saveBuffer(Currentbuf, f, true);
  fclose(f);
  exec_cmd(myEditor(Editor, shell_quote(tmpf), cur_real_linenumber(Currentbuf))
               ->ptr);
  unlink(tmpf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer") {
  Anchor *a;
  Url u;
  const char *url;

  if (Currentbuf->layout.firstLine == nullptr)
    return;

  // a = retrieveCurrentMap(Currentbuf);
  // if (a) {
  //   _followForm(false);
  //   return;
  // }
  a = retrieveCurrentAnchor(Currentbuf);
  if (a == nullptr) {
    _followForm(false);
    return;
  }
  if (*a->url == '#') { /* index within this buffer */
    gotoLabel(a->url + 1);
    return;
  }
  u = urlParse(a->url, baseURL(Currentbuf));
  if (u.to_Str() == Currentbuf->info->currentURL.to_Str()) {
    /* index within this buffer */
    if (u.label.size()) {
      gotoLabel(u.label.c_str());
      return;
    }
  }
  if (handleMailto(a->url))
    return;
  url = a->url;

  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    Buffer *buf;

    _newT();
    buf = Currentbuf;
    loadLink(url, a->target, a->referer, nullptr);
    if (buf != Currentbuf)
      CurrentTab->deleteBuffer(buf);
    else
      deleteTab(CurrentTab);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  loadLink(url, a->target, a->referer, nullptr);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer") {
  Anchor *a;
  Buffer *buf;

  if (Currentbuf->layout.firstLine == nullptr)
    return;

  a = retrieveCurrentImg(Currentbuf);
  if (a == nullptr)
    return;
  message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  refresh(term_io());
  buf = loadGeneralFile(a->url, baseURL(Currentbuf), {});
  if (buf == nullptr) {
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form") { _followForm(true); }

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink") {
  HmarkerList *hl = Currentbuf->layout.hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (Currentbuf->layout.firstLine == nullptr)
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
    if (Currentbuf->layout.href) {
      an = Currentbuf->layout.href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && Currentbuf->layout.formitem) {
      an = Currentbuf->layout.formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq++;
  } while (an == nullptr);

  Currentbuf->layout.gotoLine(po->line);
  Currentbuf->layout.pos = po->pos;
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink") {
  HmarkerList *hl = Currentbuf->layout.hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (Currentbuf->layout.firstLine == nullptr)
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
    if (Currentbuf->layout.href) {
      an = Currentbuf->layout.href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && Currentbuf->layout.formitem) {
      an = Currentbuf->layout.formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq--;
  } while (an == nullptr);

  Currentbuf->layout.gotoLine(po->line);
  Currentbuf->layout.pos = po->pos;
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link") {
  HmarkerList *hl = Currentbuf->layout.hmarklist;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (Currentbuf->layout.firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  auto po = hl->marks + n - 1;

  Anchor *an = nullptr;
  if (Currentbuf->layout.href) {
    an = Currentbuf->layout.href->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr && Currentbuf->layout.formitem) {
    an = Currentbuf->layout.formitem->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr)
    return;

  Currentbuf->layout.gotoLine(po->line);
  Currentbuf->layout.pos = po->pos;
  Currentbuf->layout.arrangeCursor();
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink") { _nextA(false); }

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink") { _prevA(false); }

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink") {
  _nextA(true);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink") {
  _prevA(true);
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink") { nextX(-1, 0); }

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink") {
  nextX(-1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink") { nextX(1, 0); }

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink") {
  nextX(1, 1);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink") { nextY(1); }

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink") { nextY(-1); }

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Switch to the next buffer") {
  Buffer *buf;
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
DEFUN(prevBf, PREV, "Switch to the previous buffer") {
  Buffer *buf;
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
DEFUN(backBf, BACK,
      "Close current buffer and return to the one below in stack") {
  // Buffer *buf = Currentbuf->linkBuffer[LB_N_FRAME];

  if (!checkBackBuffer(Currentbuf)) {
    if (close_tab_back && nTab >= 1) {
      deleteTab(CurrentTab);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
    } else
      /* FIXME: gettextize? */
      disp_message("Can't go back...", true);
    return;
  }

  CurrentTab->deleteBuffer(Currentbuf);

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local CGI-scripts)") {
  Buffer *buf = Currentbuf->nextBuffer;
  if (buf) {
    CurrentTab->deleteBuffer(buf);
  }
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer") {
  goURL0("Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer") {
  const char *url;
  if ((url = getenv("HTTP_HOME")) != nullptr ||
      (url = getenv("WWW_HOME")) != nullptr) {
    Url p_url;
    Buffer *cur_buf = Currentbuf;
    SKIP_BLANKS(url);
    url = Strnew(url_quote(url))->ptr;
    p_url = urlParse(url);
    pushHashHist(URLHist, p_url.to_Str().c_str());
    cmd_loadURL(url, {}, nullptr, nullptr);
    if (Currentbuf != cur_buf) /* success */
      pushHashHist(URLHist, Currentbuf->info->currentURL.to_Str().c_str());
  }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address") {
  goURL0("Goto relative URL: ", true);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks") {
  cmd_loadURL(BookmarkFile, {}, NO_REFERER, nullptr);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks") {
  Str *tmp;
  FormList *request;

  tmp = Sprintf(
      "mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
      (Str_form_quote(localCookie()))->ptr,
      (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
      (Str_form_quote(Strnew(Currentbuf->info->currentURL.to_Str())))->ptr,
      (Str_form_quote(Strnew(Currentbuf->layout.title)))->ptr);
  request =
      newFormList(nullptr, "post", nullptr, nullptr, nullptr, nullptr, nullptr);
  request->body = tmp->ptr;
  request->length = tmp->length;
  cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, {}, NO_REFERER, request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel") {
  cmd_loadBuffer(load_option_panel(), LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option") {
  const char *opt;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  opt = searchKeyData();
  if (opt == nullptr || *opt == '\0' || strchr(opt, '=') == nullptr) {
    if (opt != nullptr && *opt != '\0') {
      auto v = get_param_option(opt);
      opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
    }
    // opt = inputStrHist("Set option: ", opt, TextHist);
    if (opt == nullptr || *opt == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages") {
  cmd_loadBuffer(message_list_panel(), LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document") {
  Buffer *buf;

  if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != nullptr) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if ((buf = Currentbuf->linkBuffer[LB_INFO]) != nullptr) {
    CurrentTab->deleteBuffer(buf);
  }
  buf = page_info_panel(Currentbuf);
  cmd_loadBuffer(buf, LB_INFO);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced") {
  Buffer *buf;

  buf = link_list_panel(Currentbuf);
  if (buf != nullptr) {
    cmd_loadBuffer(buf, LB_NOLINK);
  }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list") {
  Buffer *buf;

  buf = cookie_list_panel();
  if (buf != nullptr)
    cmd_loadBuffer(buf, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history") {
  cmd_loadBuffer(historyBuffer(URLHist), LB_NOLINK);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target") {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  do_download = true;
  followA();
  do_download = false;
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image") {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  do_download = true;
  followI();
  do_download = false;
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document") {
  const char *qfile = nullptr, *file;
  FILE *f;
  int is_pipe;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  file = searchKeyData();
  if (file == nullptr || *file == '\0') {
    // qfile = inputLineHist("Save buffer to: ", nullptr, IN_COMMAND, SaveHist);
    if (qfile == nullptr || *qfile == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  file = qfile ? qfile : file;
  if (*file == '|') {
    is_pipe = true;
    f = popen(file + 1, "w");
  } else {
    if (qfile) {
      file = unescape_spaces(Strnew_charp(qfile))->ptr;
    }
    file = expandPath((char *)file);
    // if (checkOverWrite(file) < 0) {
    if (false) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    f = fopen(file, "w");
    is_pipe = false;
  }
  if (f == nullptr) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't open %s", file)->ptr;
    disp_err_message(emsg, true);
    return;
  }
  saveBuffer(Currentbuf, f, true);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source") {
  if (Currentbuf->info->sourcefile.empty()) {
    return;
  }
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;

  const char *file;
  if (Currentbuf->info->currentURL.schema== SCM_LOCAL) {
    file = guess_filename(Currentbuf->info->currentURL.real_file.c_str());
  } else {
    file = Currentbuf->info->guess_save_name(
        Currentbuf->info->currentURL.file.c_str());
  }
  doFileCopy(Currentbuf->info->sourcefile.c_str(), file);
  PermitSaveToPipe = false;
  displayBuffer(Currentbuf, B_NORMAL);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address") { _peekURL(0); }

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address") { _peekURL(1); }

DEFUN(curURL, PEEK, "Show current address") {
  static Str *s = nullptr;
  static int offset = 0, n;

  if (CurrentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = currentURL();
    if (DecodeURL)
      s = Strnew_charp(url_decode0(s->ptr));
  }
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message(&s->ptr[offset], true);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed") {
  Buffer *buf;

  if (Currentbuf->info->type == nullptr)
    return;
  if ((buf = Currentbuf->linkBuffer[LB_SOURCE])) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (Currentbuf->info->sourcefile.empty()) {
    return;
  }

  buf = new Buffer(INIT_BUFFER_WIDTH());

  if (is_html_type(Currentbuf->info->type)) {
    buf->info->type = "text/plain";
    if (Currentbuf->info->real_type &&
        is_html_type(Currentbuf->info->real_type))
      buf->info->real_type = "text/plain";
    else
      buf->info->real_type = Currentbuf->info->real_type;
    buf->layout.title =
        Sprintf("source of %s", Currentbuf->layout.title.c_str())->ptr;
    buf->linkBuffer[LB_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_SOURCE] = buf;
  } else if (!strcasecmp(Currentbuf->info->type, "text/plain")) {
    buf->info->type = "text/html";
    if (Currentbuf->info->real_type &&
        !strcasecmp(Currentbuf->info->real_type, "text/plain"))
      buf->info->real_type = "text/html";
    else
      buf->info->real_type = Currentbuf->info->real_type;
    buf->layout.title =
        Sprintf("HTML view of %s", Currentbuf->layout.title.c_str())->ptr;
    buf->linkBuffer[LB_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_SOURCE] = buf;
  } else {
    return;
  }
  buf->info->currentURL = Currentbuf->info->currentURL;
  buf->info->filename = Currentbuf->info->filename;
  buf->info->sourcefile = Currentbuf->info->sourcefile;
  buf->clone = Currentbuf->clone;
  buf->clone->count++;

  buf->layout.need_reshape = true;
  reshapeBuffer(buf);
  pushBuffer(buf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew") {
  Str *url;
  FormList *request;
  int multipart;

  if (Currentbuf->info->currentURL.schema == SCM_LOCAL &&
      Currentbuf->info->currentURL.file == "-") {
    /* file is std input */
    disp_err_message("Can't reload stdin", true);
    return;
  }
  auto sbuf = new Buffer(0);
  *sbuf = *Currentbuf;

  multipart = 0;
  if (Currentbuf->layout.form_submit) {
    request = Currentbuf->layout.form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      Str *query;
      struct stat st;
      multipart = 1;
      query_from_followform(&query, Currentbuf->layout.form_submit, multipart);
      stat(request->body, &st);
      request->length = st.st_size;
    }
  } else {
    request = nullptr;
  }
  url = Strnew(Currentbuf->info->currentURL.to_Str());
  message("Reloading...", 0, 0);
  refresh(term_io());
  DefaultType = Currentbuf->info->real_type;
  auto buf = loadGeneralFile(
      url->ptr, {}, {.referer = NO_REFERER, .no_cache = true}, request);
  DefaultType = nullptr;

  if (multipart)
    unlink(request->body);
  if (buf == nullptr) {
    disp_err_message("Can't reload...", true);
    return;
  } else if (buf == NO_BUFFER) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  repBuffer(Currentbuf, buf);
  if ((buf->info->type != nullptr) && (sbuf->info->type != nullptr) &&
      ((!strcasecmp(buf->info->type, "text/plain") &&
        is_html_type(sbuf->info->type)) ||
       (is_html_type(buf->info->type) &&
        !strcasecmp(sbuf->info->type, "text/plain")))) {
    vwSrc();
    if (Currentbuf != buf) {
      CurrentTab->deleteBuffer(buf);
    }
  }
  Currentbuf->layout.form_submit = sbuf->layout.form_submit;
  if (Currentbuf->layout.firstLine) {
    Currentbuf->layout.COPY_BUFROOT_FROM(sbuf->layout);
    Currentbuf->layout.restorePosition(sbuf->layout);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document") {
  Currentbuf->layout.need_reshape = true;
  reshapeBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks") {
  chkURLBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink") {
  char *p;
  int spos, epos;
  p = getCurWord(Currentbuf, &spos, &epos);
  if (p == nullptr)
    return;
  reAnchorWord(Currentbuf, Currentbuf->layout.currentLine, spos, epos);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames") {}

DEFUN(extbrz, EXTERN, "Display using an external browser") {
  if (Currentbuf->info->currentURL.schema == SCM_LOCAL &&
      Currentbuf->info->currentURL.file == "-") {
    /* file is std input */
    disp_err_message("Can't browse stdin", true);
    return;
  }
  invoke_browser(Currentbuf->info->currentURL.to_Str().c_str());
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser") {
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  auto a = retrieveCurrentAnchor(Currentbuf);
  if (a == nullptr)
    return;
  auto pu = urlParse(a->url, baseURL(Currentbuf));
  invoke_browser(pu.to_Str().c_str());
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document") {
  Line *l = Currentbuf->layout.currentLine;
  Str *tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != nullptr) {
    cur = l->real_linenumber;
    col = l->bwidth + Currentbuf->layout.currentColumn +
          Currentbuf->layout.cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    len = l->bwidth + l->width();
  }
  if (Currentbuf->layout.lastLine)
    all = Currentbuf->layout.lastLine->real_linenumber;
  tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col,
                len);

  disp_message(tmp->ptr, false);
}

DEFUN(dispVer, VERSION, "Display the version of w3m") {
  disp_message(Sprintf("w3m version %s", w3m_version)->ptr, true);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches") {
  if (WrapSearch) {
    WrapSearch = false;
    disp_message("Wrap search off", true);
  } else {
    WrapSearch = true;
    disp_message("Wrap search on", true);
  }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)") {
  // execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
      "Execute dictionary command for word at cursor") {
  execdict(GetWord(Currentbuf));
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)") {
  const char *data, *p;
  int cmd;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
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
    p = getWord(&data);
    cmd = getFuncList((char *)p);
    if (cmd < 0)
      break;
    p = getQWord(&data);
    CurrentKey = -1;
    CurrentKeyData = nullptr;
    CurrentCmdData = *p ? (char *)p : nullptr;
    w3mFuncList[cmd].func();
    CurrentCmdData = nullptr;
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setAlarm, ALARM, "Set alarm") {
  const char *data;
  int sec = 0, cmd = -1;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == nullptr) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if (*data != '\0') {
    sec = atoi(getWord(&data));
    if (sec > 0)
      cmd = getFuncList(getWord(&data));
  }
  if (cmd >= 0) {
    data = getQWord(&data);
    setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, (void *)data);
    disp_message_nsec(
        Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr, false, 1,
        false, true);
  } else {
    setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, FUNCNAME_nulcmd, nullptr);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(reinit, REINIT, "Reload configuration file") {
  auto resource = searchKeyData();

  if (resource == nullptr) {
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
    initKeymap(true);
    return;
  }

  if (!strcasecmp(resource, "MAILCAP")) {
    initMailcap();
    return;
  }

  if (!strcasecmp(resource, "MIMETYPES")) {
    initMimeTypes();
    return;
  }

  disp_err_message(
      Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, false);
}

DEFUN(defKey, DEFINE_KEY,
      "Define a binding between a key stroke combination and a command") {
  const char *data;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data == nullptr || *data == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  setKeymap(allocStr(data, -1), -1, true);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)") {
  _newT();
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(closeT, CLOSE_TAB, "Close tab") {
  TabBuffer *tab;

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

DEFUN(nextT, NEXT_TAB, "Switch to the next tab") {
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

DEFUN(prevT, PREV_TAB, "Switch to the previous tab") {
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

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab") {
  followTab(prec_num ? numTab(PREC_NUM) : nullptr);
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto URL on new tab: ", false);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto relative URL on new tab: ", true);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar") {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : LastTab, true);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar") {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : FirstTab, false);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel") {
  Buffer *buf;
  int replace = false, new_tab = false;
  int reload;

  if (!FirstDL) {
    if (replace) {
      if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == nullptr) {
        if (nTab > 1)
          deleteTab(CurrentTab);
      } else
        CurrentTab->deleteBuffer(Currentbuf);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
    }
    return;
  }
  reload = checkDownloadList();
  buf = DownloadListBuffer();
  if (!buf) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (replace) {
    buf->layout.COPY_BUFROOT_FROM(Currentbuf->layout);
    buf->layout.restorePosition(Currentbuf->layout);
  }
  if (!replace && open_tab_dl_list) {
    _newT();
    new_tab = true;
  }
  pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf();
  if (reload)
    Currentbuf->layout.event = setAlarmEvent(
        Currentbuf->layout.event, 1, AL_IMPLICIT, FUNCNAME_reload, nullptr);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement") {
  BufferPos *b = Currentbuf->layout.undo;
  if (!Currentbuf->layout.firstLine)
    return;
  if (!b || !b->prev)
    return;
  for (int i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
    ;
  resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo") {
  BufferPos *b = Currentbuf->layout.undo;
  if (!Currentbuf->layout.firstLine)
    return;
  if (!b || !b->next)
    return;
  for (int i = 0; i < PREC_NUM && b->next; i++, b = b->next)
    ;
  resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen") {
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  Currentbuf->layout.currentLine =
      Currentbuf->layout.lineSkip(Currentbuf->layout.topLine, 0, false);
  Currentbuf->layout.arrangeLine();
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen") {
  int offsety;
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  offsety = (Currentbuf->layout.LINES - 1) / 2;
  Currentbuf->layout.currentLine =
      Currentbuf->layout.topLine->currentLineSkip(offsety, false);
  Currentbuf->layout.arrangeLine();
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen") {
  int offsety;
  if (Currentbuf->layout.firstLine == nullptr)
    return;
  offsety = Currentbuf->layout.LINES - 1;
  Currentbuf->layout.currentLine =
      Currentbuf->layout.topLine->currentLineSkip(offsety, false);
  Currentbuf->layout.arrangeLine();
  displayBuffer(Currentbuf, B_NORMAL);
}
