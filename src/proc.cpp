#include "proto.h"
#include "file_util.h"
#include "readbuffer.h"
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

// NOTHING nullptr @ @ @
//"Do nothing"
void nulcmd() { /* do nothing */
}

/* Move page forward */
// NEXT_PAGE
//"Scroll down one page"
void pgFore() {
  if (vi_prec_num)
    nscroll(searchKeyNum() * (CurrentTab->currentBuffer()->layout.LINES - 1),
            B_NORMAL);
  else
    nscroll(prec_num ? searchKeyNum()
                     : searchKeyNum() *
                           (CurrentTab->currentBuffer()->layout.LINES - 1),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
// PREV_PAGE
//"Scroll up one page"
void pgBack() {
  if (vi_prec_num)
    nscroll(-searchKeyNum() * (CurrentTab->currentBuffer()->layout.LINES - 1),
            B_NORMAL);
  else
    nscroll(-(prec_num ? searchKeyNum()
                       : searchKeyNum() *
                             (CurrentTab->currentBuffer()->layout.LINES - 1)),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
// NEXT_HALF_PAGE
//"Scroll down half a page"
void hpgFore() {
  nscroll(searchKeyNum() * (CurrentTab->currentBuffer()->layout.LINES / 2 - 1),
          B_NORMAL);
}

/* Move half page backward */
// PREV_HALF_PAGE
//"Scroll up half a page"
void hpgBack() {
  nscroll(-searchKeyNum() * (CurrentTab->currentBuffer()->layout.LINES / 2 - 1),
          B_NORMAL);
}

/* 1 line up */
// UP
//"Scroll the screen up one line"
void lup1() { nscroll(searchKeyNum(), B_SCROLL); }

/* 1 line down */
// DOWN
//"Scroll the screen down one line"
void ldown1() { nscroll(-searchKeyNum(), B_SCROLL); }

/* move cursor position to the center of screen */
// CENTER_V
//"Center on cursor line"
void ctrCsrV() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  offsety = CurrentTab->currentBuffer()->layout.LINES / 2 -
            CurrentTab->currentBuffer()->layout.cursorY;
  if (offsety != 0) {
    CurrentTab->currentBuffer()->layout.topLine =
        CurrentTab->currentBuffer()->layout.lineSkip(
            CurrentTab->currentBuffer()->layout.topLine, -offsety, false);
    CurrentTab->currentBuffer()->layout.arrangeLine();
    displayBuffer(B_NORMAL);
  }
}

// CENTER_H
//"Center on cursor column"
void ctrCsrH() {
  int offsetx;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  offsetx = CurrentTab->currentBuffer()->layout.cursorX -
            CurrentTab->currentBuffer()->layout.COLS / 2;
  if (offsetx != 0) {
    CurrentTab->currentBuffer()->layout.columnSkip(offsetx);
    CurrentTab->currentBuffer()->layout.arrangeCursor();
    displayBuffer(B_NORMAL);
  }
}

/* Redraw screen */
// REDRAW
//"Draw the screen anew"
void rdrwSc() {
  clear();
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_FORCE_REDRAW);
}

// SEARCH SEARCH_FORE WHEREIS
//"Search forward"
void srchfor() { srch(forwardSearch, "Forward: "); }

// ISEARCH
//"Incremental search forward"
void isrchfor() { isrch(forwardSearch, "I-search: "); }

/* Search regular expression backward */

// SEARCH_BACK
//"Search backward"
void srchbak() { srch(backwardSearch, "Backward: "); }

// ISEARCH_BACK
//"Incremental search backward"
void isrchbak() { isrch(backwardSearch, "I-search backward: "); }

/* Search next matching */
// SEARCH_NEXT
//"Continue search forward"
void srchnxt() { srch_nxtprv(0); }

/* Search previous matching */
// SEARCH_PREV
//"Continue search backward"
void srchprv() { srch_nxtprv(1); }

/* Shift screen left */
// SHIFT_LEFT
//"Shift screen left"
void shiftl() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  int column = CurrentTab->currentBuffer()->layout.currentColumn;
  CurrentTab->currentBuffer()->layout.columnSkip(
      searchKeyNum() * (-CurrentTab->currentBuffer()->layout.COLS + 1) + 1);
  CurrentTab->currentBuffer()->layout.shiftvisualpos(
      CurrentTab->currentBuffer()->layout.currentColumn - column);
  displayBuffer(B_NORMAL);
}

/* Shift screen right */
// SHIFT_RIGHT
//"Shift screen right"
void shiftr() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  int column = CurrentTab->currentBuffer()->layout.currentColumn;
  CurrentTab->currentBuffer()->layout.columnSkip(
      searchKeyNum() * (CurrentTab->currentBuffer()->layout.COLS - 1) - 1);
  CurrentTab->currentBuffer()->layout.shiftvisualpos(
      CurrentTab->currentBuffer()->layout.currentColumn - column);
  displayBuffer(B_NORMAL);
}

// RIGHT
//"Shift screen one column right"
void col1R() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, column, n = searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    column = buf->layout.currentColumn;
    CurrentTab->currentBuffer()->layout.columnSkip(1);
    if (column == buf->layout.currentColumn)
      break;
    CurrentTab->currentBuffer()->layout.shiftvisualpos(1);
  }
  displayBuffer(B_NORMAL);
}

// LEFT
//"Shift screen one column left"
void col1L() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, n = searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    if (buf->layout.currentColumn == 0)
      break;
    CurrentTab->currentBuffer()->layout.columnSkip(-1);
    CurrentTab->currentBuffer()->layout.shiftvisualpos(-1);
  }
  displayBuffer(B_NORMAL);
}

// SETENV
//"Set environment variable"
void setEnv() {
  const char *env;
  const char *var, *value;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  env = searchKeyData();
  if (env == nullptr || *env == '\0' || strchr(env, '=') == nullptr) {
    if (env != nullptr && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    // env = inputStrHist("Set environ: ", env, TextHist);
    if (env == nullptr || *env == '\0') {
      displayBuffer(B_NORMAL);
      return;
    }
  }
  if ((value = strchr(env, '=')) != nullptr && value > env) {
    var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
  displayBuffer(B_NORMAL);
}

// PIPE_BUF
//"Pipe current buffer through a shell command and display output"
void pipeBuf() {}

/* Execute shell command and read output ac pipe. */
// PIPE_SHELL
//"Execute shell command and display output"
void pipesh() {}

/* Execute shell command and load entire output to buffer */
// READ_SHELL
//"Execute shell command and display output"
void readsh() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd == nullptr || *cmd == '\0') {
    displayBuffer(B_NORMAL);
    return;
  }
  // MySignalHandler prevtrap = {};
  // prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  auto buf = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  term_raw();
  if (buf == nullptr) {
    disp_message("Execution failed", true);
    return;
  } else {
    if (buf->res->type.empty()) {
      buf->res->type = "text/plain";
    }
    CurrentTab->pushBuffer(buf);
  }
  displayBuffer(B_FORCE_REDRAW);
}

/* Execute shell command */
// EXEC_SHELL SHELL
//"Execute shell command and display output"
void execsh() {
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
  displayBuffer(B_FORCE_REDRAW);
}

/* Load file */
// LOAD
//"Open local file in a new buffer"
void ldfile() {
  auto fn = searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn == nullptr || *fn == '\0') {
    displayBuffer(B_NORMAL);
    return;
  }
  cmd_loadfile(fn);
}

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

/* Load help file */
// HELP
//"Show help panel"
void ldhelp() {
  auto lang = AcceptLang;
  auto n = strcspn(lang, ";, \t");
  auto tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  CurrentTab->cmd_loadURL(tmp->ptr, {}, NO_REFERER, nullptr);
}

// MOVE_LEFT
//"Cursor left"
void movL() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
}

// MOVE_LEFT1
//"Cursor left. With edge touched, slide"
void movL1() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(1, m);
}

// MOVE_DOWN
//"Cursor down"
void movD() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
}

// MOVE_DOWN1
//"Cursor down. With edge touched, slide"
void movD1() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(1, m);
}

// MOVE_UP
//"Cursor up"
void movU() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
}

// MOVE_UP1
//"Cursor up. With edge touched, slide"
void movU1() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(1, m);
}

// MOVE_RIGHT
//"Cursor right"
void movR() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
}

// MOVE_RIGHT1
//"Cursor right. With edge touched, slide"
void movR1() {
  int m = searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(1, m);
}

// PREV_WORD
//"Move to the previous word"
void movLW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = CurrentTab->currentBuffer()->layout.currentLine;
    ppos = CurrentTab->currentBuffer()->layout.pos;

    if (CurrentTab->currentBuffer()->layout.prev_nonnull_line(
            CurrentTab->currentBuffer()->layout.currentLine) < 0)
      goto end;

    while (1) {
      l = CurrentTab->currentBuffer()->layout.currentLine;
      lb = l->lineBuf.data();
      while (CurrentTab->currentBuffer()->layout.pos > 0) {
        int tmp = CurrentTab->currentBuffer()->layout.pos;
        prevChar(tmp, l);
        if (is_wordchar(lb[tmp])) {
          break;
        }
        CurrentTab->currentBuffer()->layout.pos = tmp;
      }
      if (CurrentTab->currentBuffer()->layout.pos > 0)
        break;
      if (CurrentTab->currentBuffer()->layout.prev_nonnull_line(
              CurrentTab->currentBuffer()->layout.currentLine->prev) < 0) {
        CurrentTab->currentBuffer()->layout.currentLine = pline;
        CurrentTab->currentBuffer()->layout.pos = ppos;
        goto end;
      }
      CurrentTab->currentBuffer()->layout.pos =
          CurrentTab->currentBuffer()->layout.currentLine->len;
    }

    l = CurrentTab->currentBuffer()->layout.currentLine;
    lb = l->lineBuf.data();
    while (CurrentTab->currentBuffer()->layout.pos > 0) {
      int tmp = CurrentTab->currentBuffer()->layout.pos;
      prevChar(tmp, l);
      if (!is_wordchar(lb[tmp])) {
        break;
      }
      CurrentTab->currentBuffer()->layout.pos = tmp;
    }
  }
end:
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

// NEXT_WORD
//"Move to the next word"
void movRW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = CurrentTab->currentBuffer()->layout.currentLine;
    ppos = CurrentTab->currentBuffer()->layout.pos;

    if (CurrentTab->currentBuffer()->layout.next_nonnull_line(
            CurrentTab->currentBuffer()->layout.currentLine) < 0)
      goto end;

    l = CurrentTab->currentBuffer()->layout.currentLine;
    lb = l->lineBuf.data();
    while (CurrentTab->currentBuffer()->layout.pos < l->len &&
           is_wordchar(lb[CurrentTab->currentBuffer()->layout.pos])) {
      nextChar(CurrentTab->currentBuffer()->layout.pos, l);
    }

    while (1) {
      while (CurrentTab->currentBuffer()->layout.pos < l->len &&
             !is_wordchar(lb[CurrentTab->currentBuffer()->layout.pos])) {
        nextChar(CurrentTab->currentBuffer()->layout.pos, l);
      }
      if (CurrentTab->currentBuffer()->layout.pos < l->len)
        break;
      if (CurrentTab->currentBuffer()->layout.next_nonnull_line(
              CurrentTab->currentBuffer()->layout.currentLine->next) < 0) {
        CurrentTab->currentBuffer()->layout.currentLine = pline;
        CurrentTab->currentBuffer()->layout.pos = ppos;
        goto end;
      }
      CurrentTab->currentBuffer()->layout.pos = 0;
      l = CurrentTab->currentBuffer()->layout.currentLine;
      lb = l->lineBuf.data();
    }
  }
end:
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* Quit */
// ABORT EXIT
//"Quit without confirmation"
void quitfm() { _quitfm(false); }

/* Question and Quit */
// QUIT
//"Quit with confirmation request"
void qquitfm() { _quitfm(confirm_on_quit); }

/* Select buffer */
// SELECT
//"Display buffer-stack panel"
void selBuf() {

  auto ok = false;
  do {
    char cmd;
    auto buf = selectBuffer(CurrentTab->firstBuffer,
                            CurrentTab->currentBuffer(), &cmd);
    ok = CurrentTab->select(cmd, buf);
  } while (!ok);

  displayBuffer(B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
// INTERRUPT SUSPEND
//"Suspend w3m to background"
void susp() {
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
  displayBuffer(B_FORCE_REDRAW);
}

// GOTO_LINE
//"Go to the specified line"
void goLine() {

  auto str = searchKeyData();
  if (prec_num)
    CurrentTab->currentBuffer()->layout._goLine("^", prec_num);
  else if (str)
    CurrentTab->currentBuffer()->layout._goLine(str, prec_num);
  else {
    // _goLine(inputStr("Goto line: ", ""));
  }
}

// BEGIN
//"Go to the first line"
void goLineF() { CurrentTab->currentBuffer()->layout._goLine("^", prec_num); }

// END
//"Go to the last line"
void goLineL() { CurrentTab->currentBuffer()->layout._goLine("$", prec_num); }

/* Go to the beginning of the line */
// LINE_BEGIN
//"Go to the beginning of the line"
void linbeg() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  while (CurrentTab->currentBuffer()->layout.currentLine->prev &&
         CurrentTab->currentBuffer()->layout.currentLine->bpos) {
    CurrentTab->currentBuffer()->layout.cursorUp0(1);
  }
  CurrentTab->currentBuffer()->layout.pos = 0;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* Go to the bottom of the line */
// LINE_END
//"Go to the end of the line"
void linend() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  while (CurrentTab->currentBuffer()->layout.currentLine->next &&
         CurrentTab->currentBuffer()->layout.currentLine->next->bpos) {
    CurrentTab->currentBuffer()->layout.cursorDown0(1);
  }
  CurrentTab->currentBuffer()->layout.pos =
      CurrentTab->currentBuffer()->layout.currentLine->len - 1;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* Run editor on the current buffer */
// EDIT
//"Edit local source"
void editBf() {
  auto fn = CurrentTab->currentBuffer()->res->filename;

  if (fn.empty() || /* Behaving as a pager */
      (CurrentTab->currentBuffer()->res->type.empty() &&
       CurrentTab->currentBuffer()->res->edit == nullptr) || /* Reading shell */
      CurrentTab->currentBuffer()->res->currentURL.schema != SCM_LOCAL ||
      CurrentTab->currentBuffer()->res->currentURL.file ==
          "-" /* file is std input  */
  ) {
    disp_err_message("Can't edit other than local file", true);
    return;
  }

  Str *cmd;
  if (CurrentTab->currentBuffer()->res->edit) {
    cmd = unquote_mailcap(
        CurrentTab->currentBuffer()->res->edit,
        CurrentTab->currentBuffer()->res->type.c_str(), fn.c_str(),
        CurrentTab->currentBuffer()->res->getHeader("Content-Type:"), nullptr);
  } else {
    cmd = myEditor(Editor, shell_quote(fn.c_str()),
                   cur_real_linenumber(CurrentTab->currentBuffer()));
  }
  exec_cmd(cmd->ptr);

  displayBuffer(B_FORCE_REDRAW);
  reload();
}

/* Run editor on the current screen */
// EDIT_SCREEN
//"Edit rendered copy of document"
void editScr() {
  auto tmpf = tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == nullptr) {
    disp_err_message(Sprintf("Can't open %s", tmpf.c_str())->ptr, true);
    return;
  }
  saveBuffer(CurrentTab->currentBuffer(), f, true);
  fclose(f);
  exec_cmd(myEditor(Editor, shell_quote(tmpf.c_str()),
                    cur_real_linenumber(CurrentTab->currentBuffer()))
               ->ptr);
  unlink(tmpf.c_str());
  displayBuffer(B_FORCE_REDRAW);
}

/* follow HREF link */
// GOTO_LINK
//"Follow current hyperlink in a new buffer"
void followA() {
  Anchor *a;
  Url u;
  const char *url;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;

  // a = retrieveCurrentMap(CurrentTab->currentBuffer());
  // if (a) {
  //   _followForm(false);
  //   return;
  // }
  a = retrieveCurrentAnchor(&CurrentTab->currentBuffer()->layout);
  if (a == nullptr) {
    CurrentTab->_followForm(false);
    return;
  }
  if (*a->url == '#') { /* index within this buffer */
    gotoLabel(a->url + 1);
    return;
  }
  u = urlParse(a->url, CurrentTab->currentBuffer()->res->getBaseURL());
  if (u.to_Str() == CurrentTab->currentBuffer()->res->currentURL.to_Str()) {
    /* index within this buffer */
    if (u.label.size()) {
      gotoLabel(u.label.c_str());
      return;
    }
  }
  url = a->url;

  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    TabBuffer::_newT();
    auto buf = CurrentTab->currentBuffer();
    CurrentTab->loadLink(url, a->target, a->referer, nullptr);
    if (buf != CurrentTab->currentBuffer())
      CurrentTab->deleteBuffer(buf);
    else
      deleteTab(CurrentTab);
    displayBuffer(B_FORCE_REDRAW);
    return;
  }
  CurrentTab->loadLink(url, a->target, a->referer, nullptr);
  displayBuffer(B_NORMAL);
}

/* view inline image */
// VIEW_IMAGE
//"Display image in viewer"
void followI() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;

  auto a = retrieveCurrentImg(&CurrentTab->currentBuffer()->layout);
  if (a == nullptr)
    return;
  message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  refresh(term_io());

  auto res = loadGeneralFile(
      a->url, CurrentTab->currentBuffer()->res->getBaseURL(), {});
  if (!res) {
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    disp_err_message(emsg, false);
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
  displayBuffer(B_NORMAL);
}

/* submit form */
// SUBMIT
//"Submit form"
void submitForm() { CurrentTab->_followForm(true); }

/* go to the top anchor */
// LINK_BEGIN
//"Move to the first hyperlink"
void topA() {
  HmarkerList *hl = CurrentTab->currentBuffer()->layout.hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
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
    if (CurrentTab->currentBuffer()->layout.href) {
      an = CurrentTab->currentBuffer()->layout.href->retrieveAnchor(po->line,
                                                                    po->pos);
    }
    if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem) {
      an = CurrentTab->currentBuffer()->layout.formitem->retrieveAnchor(
          po->line, po->pos);
    }
    hseq++;
  } while (an == nullptr);

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* go to the last anchor */
// LINK_END
//"Move to the last hyperlink"
void lastA() {
  HmarkerList *hl = CurrentTab->currentBuffer()->layout.hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
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
    if (CurrentTab->currentBuffer()->layout.href) {
      an = CurrentTab->currentBuffer()->layout.href->retrieveAnchor(po->line,
                                                                    po->pos);
    }
    if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem) {
      an = CurrentTab->currentBuffer()->layout.formitem->retrieveAnchor(
          po->line, po->pos);
    }
    hseq--;
  } while (an == nullptr);

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* go to the nth anchor */
// LINK_N
//"Go to the nth link"
void nthA() {
  HmarkerList *hl = CurrentTab->currentBuffer()->layout.hmarklist;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  auto po = hl->marks + n - 1;

  Anchor *an = nullptr;
  if (CurrentTab->currentBuffer()->layout.href) {
    an = CurrentTab->currentBuffer()->layout.href->retrieveAnchor(po->line,
                                                                  po->pos);
  }
  if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem) {
    an = CurrentTab->currentBuffer()->layout.formitem->retrieveAnchor(po->line,
                                                                      po->pos);
  }
  if (an == nullptr)
    return;

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  displayBuffer(B_NORMAL);
}

/* go to the next anchor */
// NEXT_LINK
//"Move to the next hyperlink"
void nextA() {
  int n = searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(false, baseUr, n);
}

/* go to the previous anchor */
// PREV_LINK
//"Move to the previous hyperlink"
void prevA() {
  int n = searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(false, baseUr, n);
}

/* go to the next visited anchor */
// NEXT_VISITED
//"Move to the next visited hyperlink"
void nextVA() {
  int n = searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(true, baseUr, n);
}

/* go to the previous visited anchor */
// PREV_VISITED
//"Move to the previous visited hyperlink"
void prevVA() {
  int n = searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(true, baseUr, n);
}

/* go to the next left anchor */
// NEXT_LEFT
//"Move left to the next hyperlink"
void nextL() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, 0, n);
}

/* go to the next left-up anchor */
// NEXT_LEFT_UP
//"Move left or upward to the next hyperlink"
void nextLU() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, -1, n);
}

/* go to the next right anchor */
// NEXT_RIGHT
//"Move right to the next hyperlink"
void nextR() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 0, n);
}

/* go to the next right-down anchor */
// NEXT_RIGHT_DOWN
//"Move right or downward to the next hyperlink"
void nextRD() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 1, n);
}

/* go to the next downward anchor */
// NEXT_DOWN
//"Move downward to the next hyperlink"
void nextD() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextY(1, n);
}

/* go to the next upward anchor */
// NEXT_UP
//"Move upward to the next hyperlink"
void nextU() {
  int n = searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextY(-1, n);
}

/* go to the next bufferr */
// NEXT
//"Switch to the next buffer"
void nextBf() {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf =
        forwardBuffer(CurrentTab->firstBuffer, CurrentTab->currentBuffer());
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
  displayBuffer(B_FORCE_REDRAW);
}

/* go to the previous bufferr */
// PREV
//"Switch to the previous buffer"
void prevBf() {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf = CurrentTab->currentBuffer()->backBuffer;
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
  displayBuffer(B_FORCE_REDRAW);
}

/* delete current buffer and back to the previous buffer */
// BACK
//"Close current buffer and return to the one below in stack"
void backBf() {
  // Buffer *buf = CurrentTab->currentBuffer()->linkBuffer[LB_N_FRAME];

  if (!checkBackBuffer(CurrentTab->currentBuffer())) {
    if (close_tab_back && nTab >= 1) {
      deleteTab(CurrentTab);
      displayBuffer(B_FORCE_REDRAW);
    } else
      disp_message("Can't go back...", true);
    return;
  }

  CurrentTab->deleteBuffer(CurrentTab->currentBuffer());

  displayBuffer(B_FORCE_REDRAW);
}

// DELETE_PREVBUF
//"Delete previous buffer (mainly for local CGI-scripts)"
void deletePrevBuf() {
  auto buf = CurrentTab->currentBuffer()->backBuffer;
  if (buf) {
    CurrentTab->deleteBuffer(buf);
  }
}

// GOTO
//"Open specified document in a new buffer"
void goURL() { goURL0("Goto URL: ", false); }

// GOTO_HOME
//"Open home page in a new buffer"
void goHome() {
  const char *url;
  if ((url = getenv("HTTP_HOME")) != nullptr ||
      (url = getenv("WWW_HOME")) != nullptr) {
    Url p_url;
    auto cur_buf = CurrentTab->currentBuffer();
    SKIP_BLANKS(url);
    url = Strnew(url_quote(url))->ptr;
    p_url = urlParse(url);
    pushHashHist(URLHist, p_url.to_Str().c_str());
    CurrentTab->cmd_loadURL(url, {}, nullptr, nullptr);
    if (CurrentTab->currentBuffer() != cur_buf) /* success */
      pushHashHist(
          URLHist,
          CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
  }
}

// GOTO_RELATIVE
//"Go to relative address"
void gorURL() { goURL0("Goto relative URL: ", true); }

/* load bookmark */
// BOOKMARK VIEW_BOOKMARK
//"View bookmarks"
void ldBmark() {
  CurrentTab->cmd_loadURL(BookmarkFile, {}, NO_REFERER, nullptr);
}

/* Add current to bookmark */
// ADD_BOOKMARK
//"Add current page to bookmarks"
void adBmark() {
  Str *tmp;
  FormList *request;

  tmp = Sprintf(
      "mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
      (Str_form_quote(localCookie()))->ptr,
      (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
      (Str_form_quote(
           Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str())))
          ->ptr,
      (Str_form_quote(Strnew(CurrentTab->currentBuffer()->layout.title)))->ptr);
  request =
      newFormList(nullptr, "post", nullptr, nullptr, nullptr, nullptr, nullptr);
  request->body = tmp->ptr;
  request->length = tmp->length;
  CurrentTab->cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, {}, NO_REFERER,
                          request);
}

/* option setting */
// OPTIONS
//"Display options setting panel"
void ldOpt() { cmd_loadBuffer(load_option_panel(), LB_NOLINK); }

/* set an option */
// SET_OPTION
//"Set option"
void setOpt() {
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
      displayBuffer(B_NORMAL);
      return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
  displayBuffer(B_REDRAW_IMAGE);
}

/* error message list */
// MSGS
//"Display error messages"
void msgs() { cmd_loadBuffer(message_list_panel(), LB_NOLINK); }

/* page info */
// INFO
//"Display information about the current document"
void pginfo() {

  if (auto buf = CurrentTab->currentBuffer()->linkBuffer[LB_N_INFO]) {
    CurrentTab->currentBuffer(buf);
    displayBuffer(B_NORMAL);
    return;
  }

  if (auto buf = CurrentTab->currentBuffer()->linkBuffer[LB_INFO]) {
    CurrentTab->deleteBuffer(buf);
  }

  {
    auto buf = page_info_panel(CurrentTab->currentBuffer());
    cmd_loadBuffer(buf, LB_INFO);
  }
}

/* link,anchor,image list */
// LIST
//"Show all URLs referenced"
void linkLst() {
  auto buf = link_list_panel(CurrentTab->currentBuffer());
  if (buf != nullptr) {
    cmd_loadBuffer(buf, LB_NOLINK);
  }
}

/* cookie list */
// COOKIE
//"View cookie list"
void cooLst() {
  auto buf = cookie_list_panel();
  if (buf != nullptr) {
    cmd_loadBuffer(buf, LB_NOLINK);
  }
}

/* History page */
// HISTORY
//"Show browsing history"
void ldHist() { cmd_loadBuffer(historyBuffer(URLHist), LB_NOLINK); }

/* download HREF link */
// SAVE_LINK
//"Save hyperlink target"
void svA() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followA();
  // do_download = false;
}

/* download IMG link */
// SAVE_IMAGE
//"Save inline image"
void svI() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followI();
  // do_download = false;
}

/* save buffer */
// PRINT SAVE_SCREEN
//"Save rendered document"
void svBuf() {
  const char *qfile = nullptr, *file;
  FILE *f;
  int is_pipe;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  file = searchKeyData();
  if (file == nullptr || *file == '\0') {
    // qfile = inputLineHist("Save buffer to: ", nullptr, IN_COMMAND, SaveHist);
    if (qfile == nullptr || *qfile == '\0') {
      displayBuffer(B_NORMAL);
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
      displayBuffer(B_NORMAL);
      return;
    }
    f = fopen(file, "w");
    is_pipe = false;
  }
  if (f == nullptr) {
    char *emsg = Sprintf("Can't open %s", file)->ptr;
    disp_err_message(emsg, true);
    return;
  }
  saveBuffer(CurrentTab->currentBuffer(), f, true);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
  displayBuffer(B_NORMAL);
}

/* save source */
// DOWNLOAD SAVE
//"Save document source"
void svSrc() {
  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    return;
  }
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;

  const char *file;
  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL) {
    file = guess_filename(
        CurrentTab->currentBuffer()->res->currentURL.real_file.c_str());
  } else {
    file = CurrentTab->currentBuffer()->res->guess_save_name(
        CurrentTab->currentBuffer()->res->currentURL.file.c_str());
  }
  doFileCopy(CurrentTab->currentBuffer()->res->sourcefile.c_str(), file);
  PermitSaveToPipe = false;
  displayBuffer(B_NORMAL);
}

/* peek URL */
// PEEK_LINK
//"Show target address"
void peekURL() { _peekURL(0); }

/* peek URL of image */
// PEEK_IMG
//"Show image address"
void peekIMG() { _peekURL(1); }

// PEEK
//"Show current address"
void curURL() {
  static Str *s = nullptr;
  static int offset = 0, n;

  if (CurrentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str());
    if (DecodeURL)
      s = Strnew_charp(url_decode0(s->ptr));
  }
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message(&s->ptr[offset], true);
}
/* view HTML source */

// SOURCE VIEW
//"Toggle between HTML shown or processed"
void vwSrc() {

  if (CurrentTab->currentBuffer()->res->type.empty()) {
    return;
  }

  std::shared_ptr<Buffer> buf;
  if ((buf = CurrentTab->currentBuffer()->linkBuffer[LB_SOURCE])) {
    CurrentTab->currentBuffer(buf);
    displayBuffer(B_NORMAL);
    return;
  }
  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    return;
  }

  buf = Buffer::create();

  if (CurrentTab->currentBuffer()->res->is_html_type()) {
    buf->res->type = "text/plain";
    buf->layout.title =
        Sprintf("source of %s",
                CurrentTab->currentBuffer()->layout.title.c_str())
            ->ptr;
    buf->linkBuffer[LB_SOURCE] = CurrentTab->currentBuffer();
    CurrentTab->currentBuffer()->linkBuffer[LB_SOURCE] = buf;
  } else if (CurrentTab->currentBuffer()->res->type == "text/plain") {
    buf->res->type = "text/html";
    buf->layout.title =
        Sprintf("HTML view of %s",
                CurrentTab->currentBuffer()->layout.title.c_str())
            ->ptr;
    buf->linkBuffer[LB_SOURCE] = CurrentTab->currentBuffer();
    CurrentTab->currentBuffer()->linkBuffer[LB_SOURCE] = buf;
  } else {
    return;
  }
  buf->res->currentURL = CurrentTab->currentBuffer()->res->currentURL;
  buf->res->filename = CurrentTab->currentBuffer()->res->filename;
  buf->res->sourcefile = CurrentTab->currentBuffer()->res->sourcefile;

  buf->layout.need_reshape = true;
  reshapeBuffer(buf);
  CurrentTab->pushBuffer(buf);
  displayBuffer(B_NORMAL);
}

/* reload */
// RELOAD
//"Load current document anew"
void reload() {
  Str *url;
  FormList *request;
  int multipart;

  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    disp_err_message("Can't reload stdin", true);
    return;
  }
  auto sbuf = Buffer::create(0);
  *sbuf = *CurrentTab->currentBuffer();

  multipart = 0;
  if (CurrentTab->currentBuffer()->layout.form_submit) {
    request = CurrentTab->currentBuffer()->layout.form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      Str *query;
      struct stat st;
      multipart = 1;
      query_from_followform(
          &query, CurrentTab->currentBuffer()->layout.form_submit, multipart);
      stat(request->body, &st);
      request->length = st.st_size;
    }
  } else {
    request = nullptr;
  }
  url = Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str());
  message("Reloading...", 0, 0);
  refresh(term_io());
  DefaultType = Strnew(CurrentTab->currentBuffer()->res->type)->ptr;

  auto res = loadGeneralFile(
      url->ptr, {}, {.referer = NO_REFERER, .no_cache = true}, request);
  DefaultType = nullptr;

  if (multipart)
    unlink(request->body);
  if (!res) {
    disp_err_message("Can't reload...", true);
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf == NO_BUFFER) {
  //   displayBuffer(CurrentTab->currentBuffer(), B_NORMAL);
  //   return;
  // }
  CurrentTab->repBuffer(CurrentTab->currentBuffer(), buf);
  if ((buf->res->type.size()) && (sbuf->res->type.size()) &&
      ((buf->res->type == "text/plain" && sbuf->res->is_html_type()) ||
       (buf->res->is_html_type() && sbuf->res->type == "text/plain"))) {
    vwSrc();
    if (CurrentTab->currentBuffer() != buf) {
      CurrentTab->deleteBuffer(buf);
    }
  }
  CurrentTab->currentBuffer()->layout.form_submit = sbuf->layout.form_submit;
  if (CurrentTab->currentBuffer()->layout.firstLine) {
    CurrentTab->currentBuffer()->layout.COPY_BUFROOT_FROM(sbuf->layout);
    CurrentTab->currentBuffer()->layout.restorePosition(sbuf->layout);
  }
  displayBuffer(B_FORCE_REDRAW);
}

/* reshape */
// RESHAPE
//"Re-render document"
void reshape() {
  CurrentTab->currentBuffer()->layout.need_reshape = true;
  reshapeBuffer(CurrentTab->currentBuffer());
  displayBuffer(B_FORCE_REDRAW);
}

// MARK_URL
//"Turn URL-like strings into hyperlinks"
void chkURL() {
  chkURLBuffer(CurrentTab->currentBuffer());
  displayBuffer(B_FORCE_REDRAW);
}

// MARK_WORD
//"Turn current word into hyperlink"
void chkWORD() {
  int spos, epos;
  auto p = CurrentTab->currentBuffer()->layout.getCurWord(&spos, &epos);
  if (p.empty())
    return;
  reAnchorWord(&CurrentTab->currentBuffer()->layout,
               CurrentTab->currentBuffer()->layout.currentLine, spos, epos);
  displayBuffer(B_FORCE_REDRAW);
}

/* render frames */
// FRAME
//"Toggle rendering HTML frames"
void rFrame() {}

// EXTERN
//"Display using an external browser"
void extbrz() {
  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    disp_err_message("Can't browse stdin", true);
    return;
  }
  invoke_browser(CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
}

// EXTERN_LINK
//"Display target using an external browser"
void linkbrz() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  auto a = retrieveCurrentAnchor(&CurrentTab->currentBuffer()->layout);
  if (a == nullptr)
    return;
  auto pu = urlParse(a->url, CurrentTab->currentBuffer()->res->getBaseURL());
  invoke_browser(pu.to_Str().c_str());
}

/* show current line number and number of lines in the entire document */
// LINE_INFO
//"Display current position in document"
void curlno() {
  Line *l = CurrentTab->currentBuffer()->layout.currentLine;
  Str *tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != nullptr) {
    cur = l->real_linenumber;
    col = l->bwidth + CurrentTab->currentBuffer()->layout.currentColumn +
          CurrentTab->currentBuffer()->layout.cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    len = l->bwidth + l->width();
  }
  if (CurrentTab->currentBuffer()->layout.lastLine)
    all = CurrentTab->currentBuffer()->layout.lastLine->real_linenumber;
  tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col,
                len);

  disp_message(tmp->ptr, false);
}

// VERSION
//"Display the version of w3m"
void dispVer() {
  disp_message(Sprintf("w3m version %s", w3m_version)->ptr, true);
}

// WRAP_TOGGLE
//"Toggle wrapping mode in searches"
void wrapToggle() {
  if (WrapSearch) {
    WrapSearch = false;
    disp_message("Wrap search off", true);
  } else {
    WrapSearch = true;
    disp_message("Wrap search on", true);
  }
}

// DICT_WORD
//"Execute dictionary command (see README.dict)"
void dictword() {
  // execdict(inputStr("(dictionary)!", ""));
}

// DICT_WORD_AT
//"Execute dictionary command for word at cursor"
void dictwordat() {
  auto word = CurrentTab->currentBuffer()->layout.getCurWord();
  execdict(word.c_str());
}

// COMMAND
//"Invoke w3m function(s)"
void execCmd() {
  const char *data, *p;
  int cmd;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
      displayBuffer(B_NORMAL);
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
  displayBuffer(B_NORMAL);
}

// ALARM
//"Set alarm"
void setAlarm() {
  const char *data;
  int sec = 0, cmd = -1;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == nullptr) {
      displayBuffer(B_NORMAL);
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
  displayBuffer(B_NORMAL);
}

// REINIT
//"Reload configuration file"
void reinit() {
  auto resource = searchKeyData();

  if (resource == nullptr) {
    init_rc();
    sync_with_option();
    initCookie();
    displayBuffer(B_REDRAW_IMAGE);
    return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
    displayBuffer(B_REDRAW_IMAGE);
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

// DEFINE_KEY
//"Define a binding between a key stroke combination and a command"
void defKey() {
  const char *data;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data == nullptr || *data == '\0') {
      displayBuffer(B_NORMAL);
      return;
    }
  }
  setKeymap(allocStr(data, -1), -1, true);
  displayBuffer(B_NORMAL);
}

// NEW_TAB
//"Open a new tab (with current document)"
void newT() {
  TabBuffer::_newT();
  displayBuffer(B_REDRAW_IMAGE);
}

// CLOSE_TAB
//"Close tab"
void closeT() {
  TabBuffer *tab;

  if (nTab <= 1)
    return;
  if (prec_num)
    tab = numTab(PREC_NUM);
  else
    tab = CurrentTab;
  if (tab)
    deleteTab(tab);
  displayBuffer(B_REDRAW_IMAGE);
}

// NEXT_TAB
//"Switch to the next tab"
void nextT() {
  int i;

  if (nTab <= 1)
    return;
  for (i = 0; i < PREC_NUM; i++) {
    if (CurrentTab->nextTab)
      CurrentTab = CurrentTab->nextTab;
    else
      CurrentTab = FirstTab;
  }
  displayBuffer(B_REDRAW_IMAGE);
}

// PREV_TAB
//"Switch to the previous tab"
void prevT() {
  int i;

  if (nTab <= 1)
    return;
  for (i = 0; i < PREC_NUM; i++) {
    if (CurrentTab->prevTab)
      CurrentTab = CurrentTab->prevTab;
    else
      CurrentTab = LastTab;
  }
  displayBuffer(B_REDRAW_IMAGE);
}

// TAB_LINK
//"Follow current hyperlink in a new tab"
void tabA() { followTab(prec_num ? numTab(PREC_NUM) : nullptr); }

// TAB_GOTO
//"Open specified document in a new tab"
void tabURL() {
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto URL on new tab: ", false);
}

// TAB_GOTO_RELATIVE
//"Open relative address in a new tab"
void tabrURL() {
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto relative URL on new tab: ", true);
}

// TAB_RIGHT
//"Move right along the tab bar"
void tabR() {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : LastTab, true);
}

// TAB_LEFT
//"Move left along the tab bar"
void tabL() {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : FirstTab, false);
}

/* download panel */
// DOWNLOAD_LIST
//"Display downloads panel"
void ldDL() {
  int replace = false, new_tab = false;
  int reload;

  if (!FirstDL) {
    if (replace) {
      if (CurrentTab->currentBuffer() == CurrentTab->firstBuffer &&
          CurrentTab->currentBuffer()->backBuffer == nullptr) {
        if (nTab > 1)
          deleteTab(CurrentTab);
      } else
        CurrentTab->deleteBuffer(CurrentTab->currentBuffer());
      displayBuffer(B_FORCE_REDRAW);
    }
    return;
  }
  reload = checkDownloadList();
  auto buf = DownloadListBuffer();
  if (!buf) {
    displayBuffer(B_NORMAL);
    return;
  }
  if (replace) {
    buf->layout.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
    buf->layout.restorePosition(CurrentTab->currentBuffer()->layout);
  }
  if (!replace && open_tab_dl_list) {
    TabBuffer::_newT();
    new_tab = true;
  }
  CurrentTab->pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf();
  if (reload)
    CurrentTab->currentBuffer()->layout.event =
        setAlarmEvent(CurrentTab->currentBuffer()->layout.event, 1, AL_IMPLICIT,
                      FUNCNAME_reload, nullptr);
  displayBuffer(B_FORCE_REDRAW);
}

// UNDO
//"Cancel the last cursor movement"
void undoPos() {
  BufferPos *b = CurrentTab->currentBuffer()->layout.undo;
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    return;
  if (!b || !b->prev)
    return;
  for (int i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
    ;
  resetPos(b);
}

// REDO
//"Cancel the last undo"
void redoPos() {
  BufferPos *b = CurrentTab->currentBuffer()->layout.undo;
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    return;
  if (!b || !b->next)
    return;
  for (int i = 0; i < PREC_NUM && b->next; i++, b = b->next)
    ;
  resetPos(b);
}

// CURSOR_TOP
//"Move cursor to the top of the screen"
void cursorTop() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.lineSkip(
          CurrentTab->currentBuffer()->layout.topLine, 0, false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  displayBuffer(B_NORMAL);
}

// CURSOR_MIDDLE
//"Move cursor to the middle of the screen"
void cursorMiddle() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  offsety = (CurrentTab->currentBuffer()->layout.LINES - 1) / 2;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.topLine->currentLineSkip(offsety,
                                                                   false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  displayBuffer(B_NORMAL);
}

// CURSOR_BOTTOM
//"Move cursor to the bottom of the screen"
void cursorBottom() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  offsety = CurrentTab->currentBuffer()->layout.LINES - 1;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.topLine->currentLineSkip(offsety,
                                                                   false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  displayBuffer(B_NORMAL);
}
