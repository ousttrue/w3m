#include "proto.h"
#include "app.h"
#include "html/form_item.h"
#include "file_util.h"
#include "html/readbuffer.h"
#include "http_response.h"
#include "url_quote.h"
#include "search.h"
#include "mimetypes.h"
#include "downloadlist.h"
#include "url_stream.h"
#include "linein.h"
#include "cookie.h"
#include "rc.h"
#include "html/form.h"
#include "history.h"
#include "html/anchor.h"
#include "etc.h"
#include "mailcap.h"
#include "quote.h"
#include "http_request.h"
#include "http_response.h"
#include "http_session.h"
#include "local_cgi.h"
#include "Str.h"
#include "search.h"
#include "w3m.h"
#include "app.h"
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
  CurrentTab->currentBuffer()->layout.nscroll(
      prec_num ? App::instance().searchKeyNum()
               : App::instance().searchKeyNum() *
                     (CurrentTab->currentBuffer()->layout.LINES - 1));
}

/* Move page backward */
// PREV_PAGE
//"Scroll up one page"
void pgBack() {
  CurrentTab->currentBuffer()->layout.nscroll(
      -(prec_num ? App::instance().searchKeyNum()
                 : App::instance().searchKeyNum() *
                       (CurrentTab->currentBuffer()->layout.LINES - 1)));
}

/* Move half page forward */
// NEXT_HALF_PAGE
//"Scroll down half a page"
void hpgFore() {
  CurrentTab->currentBuffer()->layout.nscroll(
      App::instance().searchKeyNum() *
      (CurrentTab->currentBuffer()->layout.LINES / 2 - 1));
}

/* Move half page backward */
// PREV_HALF_PAGE
//"Scroll up half a page"
void hpgBack() {
  CurrentTab->currentBuffer()->layout.nscroll(
      -App::instance().searchKeyNum() *
      (CurrentTab->currentBuffer()->layout.LINES / 2 - 1));
}

/* 1 line up */
// UP
//"Scroll the screen up one line"
void lup1() {
  CurrentTab->currentBuffer()->layout.nscroll(App::instance().searchKeyNum());
}

/* 1 line down */
// DOWN
//"Scroll the screen down one line"
void ldown1() {
  CurrentTab->currentBuffer()->layout.nscroll(-App::instance().searchKeyNum());
}

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
    App::instance().invalidate();
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
    App::instance().invalidate();
  }
}

/* Redraw screen */
// REDRAW
//"Draw the screen anew"
void rdrwSc() {
  // clear();
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
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
      App::instance().searchKeyNum() *
          (-CurrentTab->currentBuffer()->layout.COLS + 1) +
      1);
  CurrentTab->currentBuffer()->layout.shiftvisualpos(
      CurrentTab->currentBuffer()->layout.currentColumn - column);
  App::instance().invalidate();
}

/* Shift screen right */
// SHIFT_RIGHT
//"Shift screen right"
void shiftr() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  int column = CurrentTab->currentBuffer()->layout.currentColumn;
  CurrentTab->currentBuffer()->layout.columnSkip(
      App::instance().searchKeyNum() *
          (CurrentTab->currentBuffer()->layout.COLS - 1) -
      1);
  CurrentTab->currentBuffer()->layout.shiftvisualpos(
      CurrentTab->currentBuffer()->layout.currentColumn - column);
  App::instance().invalidate();
}

// RIGHT
//"Shift screen one column right"
void col1R() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, column, n = App::instance().searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    column = buf->layout.currentColumn;
    CurrentTab->currentBuffer()->layout.columnSkip(1);
    if (column == buf->layout.currentColumn)
      break;
    CurrentTab->currentBuffer()->layout.shiftvisualpos(1);
  }
  App::instance().invalidate();
}

// LEFT
//"Shift screen one column left"
void col1L() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, n = App::instance().searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    if (buf->layout.currentColumn == 0)
      break;
    CurrentTab->currentBuffer()->layout.columnSkip(-1);
    CurrentTab->currentBuffer()->layout.shiftvisualpos(-1);
  }
  App::instance().invalidate();
}

// SETENV
//"Set environment variable"
void setEnv() {
  const char *env;
  const char *var, *value;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  env = App::instance().searchKeyData();
  if (env == nullptr || *env == '\0' || strchr(env, '=') == nullptr) {
    if (env != nullptr && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    // env = inputStrHist("Set environ: ", env, TextHist);
    if (env == nullptr || *env == '\0') {
      App::instance().invalidate();
      return;
    }
  }
  if ((value = strchr(env, '=')) != nullptr && value > env) {
    var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
  App::instance().invalidate();
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
  auto cmd = App::instance().searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd == nullptr || *cmd == '\0') {
    App::instance().invalidate();
    return;
  }
  // MySignalHandler prevtrap = {};
  // prevtrap = mySignal(SIGINT, intTrap);
  // crmode();
  auto buf = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  // term_raw();
  if (buf == nullptr) {
    App::instance().disp_message("Execution failed");
    return;
  } else {
    if (buf->res->type.empty()) {
      buf->res->type = "text/plain";
    }
    CurrentTab->pushBuffer(buf);
  }
  App::instance().invalidate();
}

/* Execute shell command */
// EXEC_SHELL SHELL
//"Execute shell command and display output"
void execsh() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = App::instance().searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != nullptr && *cmd != '\0') {
    App::instance().endRawMode();
    printf("\n");
    (void)!system(cmd); /* We do not care about the exit code here! */
    printf("\n[Hit any key]");
    fflush(stdout);
    App::instance().beginRawMode();
    // getch();
  }
  App::instance().invalidate();
}

/* Load file */
// LOAD
//"Open local file in a new buffer"
void ldfile() {
  auto fn = App::instance().searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn == nullptr || *fn == '\0') {
    App::instance().invalidate();
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
  CurrentTab->cmd_loadURL(tmp->ptr, {}, {.no_referer = true}, nullptr);
}

// MOVE_LEFT
//"Cursor left"
void movL() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
}

// MOVE_LEFT1
//"Cursor left. With edge touched, slide"
void movL1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(1, m);
}

// MOVE_DOWN
//"Cursor down"
void movD() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
}

// MOVE_DOWN1
//"Cursor down. With edge touched, slide"
void movD1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(1, m);
}

// MOVE_UP
//"Cursor up"
void movU() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
}

// MOVE_UP1
//"Cursor up. With edge touched, slide"
void movU1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(1, m);
}

// MOVE_RIGHT
//"Cursor right"
void movR() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
}

// MOVE_RIGHT1
//"Cursor right. With edge touched, slide"
void movR1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(1, m);
}

// PREV_WORD
//"Move to the previous word"
void movLW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = App::instance().searchKeyNum();

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
  App::instance().invalidate();
}

// NEXT_WORD
//"Move to the next word"
void movRW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = App::instance().searchKeyNum();

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
  App::instance().invalidate();
}

/* Quit */
// ABORT EXIT
//"Quit without confirmation"
void quitfm() { App::instance().exit(0); }

/* Question and Quit */
// QUIT
//"Quit with confirmation request"
void qquitfm() { App::instance().exit(0); }

/* Select buffer */
// SELECT
//"Display buffer-stack panel"
void selBuf() {

  // auto ok = false;
  // do {
  //   char cmd;
  //   auto buf = selectBuffer(CurrentTab->firstBuffer,
  //                           CurrentTab->currentBuffer(), &cmd);
  //   ok = CurrentTab->select(cmd, buf);
  // } while (!ok);
  //
  // App::instance().invalidate();
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
// INTERRUPT SUSPEND
//"Suspend w3m to background"
void susp() {
  // #ifndef SIGSTOP
  //   const char *shell;
  // #endif /* not SIGSTOP */
  //   // move(LASTLINE(), 0);
  //   clrtoeolx();
  //   // refresh(term_io());
  //   fmTerm();
  // #ifndef SIGSTOP
  //   shell = getenv("SHELL");
  //   if (shell == nullptr)
  //     shell = "/bin/sh";
  //   system(shell);
  // #else  /* SIGSTOP */
  //   signal(SIGTSTP, SIG_DFL); /* just in case */
  //   /*
  //    * Note: If susp() was called from SIGTSTP handler,
  //    * unblocking SIGTSTP would be required here.
  //    * Currently not.
  //    */
  //   kill(0, SIGTSTP); /* stop whole job, not a single process */
  // #endif /* SIGSTOP */
  //   fmInit();
  //   App::instance().invalidate();
}

// GOTO_LINE
//"Go to the specified line"
void goLine() {

  auto str = App::instance().searchKeyData();
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
  App::instance().invalidate();
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
  App::instance().invalidate();
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
    App::instance().disp_err_message("Can't edit other than local file");
    return;
  }

  std::string cmd;
  if (CurrentTab->currentBuffer()->res->edit) {
    cmd = unquote_mailcap(
              CurrentTab->currentBuffer()->res->edit,
              CurrentTab->currentBuffer()->res->type.c_str(), fn.c_str(),
              CurrentTab->currentBuffer()->res->getHeader("Content-Type:"),
              nullptr)
              ->ptr;
  } else {
    cmd = App::instance().myEditor(
        shell_quote(fn.c_str()),
        cur_real_linenumber(CurrentTab->currentBuffer()));
  }
  exec_cmd(cmd);

  App::instance().invalidate();
  reload();
}

/* Run editor on the current screen */
// EDIT_SCREEN
//"Edit rendered copy of document"
void editScr() {
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == nullptr) {
    App::instance().disp_err_message(
        Sprintf("Can't open %s", tmpf.c_str())->ptr);
    return;
  }
  saveBuffer(CurrentTab->currentBuffer(), f, true);
  fclose(f);
  exec_cmd(App::instance().myEditor(
      shell_quote(tmpf.c_str()),
      cur_real_linenumber(CurrentTab->currentBuffer())));
  unlink(tmpf.c_str());
  App::instance().invalidate();
}

/* follow HREF link */
// GOTO_LINK
//"Follow current hyperlink in a new buffer"
void followA() {

  CurrentTab->followAnchor();

  App::instance().invalidate();
}

/* view inline image */
// VIEW_IMAGE
//"Display image in viewer"
void followI() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;

  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentImg();
  if (a == nullptr)
    return;
  App::instance().message(Sprintf("loading %s", a->url)->ptr, 0, 0);

  auto res = loadGeneralFile(
      a->url, CurrentTab->currentBuffer()->res->getBaseURL(), {});
  if (!res) {
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    App::instance().disp_err_message(emsg);
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
  App::instance().invalidate();
}

/* submit form */
// SUBMIT
//"Submit form"
void submitForm() {
  if (auto f = CurrentTab->currentBuffer()->layout.retrieveCurrentForm()) {
    auto buf = CurrentTab->currentBuffer()->followForm(f, true);
    if (buf) {
      App::instance().pushBuffer(buf, f->target);
    }
  }
}

/* go to the top anchor */
// LINK_BEGIN
//"Move to the first hyperlink"
void topA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  if (hl->size() == 0)
    return;

  if (prec_num > (int)hl->size())
    hseq = hl->size() - 1;
  else if (prec_num > 0)
    hseq = prec_num - 1;
  do {
    if (hseq >= (int)hl->size())
      return;
    po = &hl->marks[hseq];
    if (CurrentTab->currentBuffer()->layout.href()) {
      an = CurrentTab->currentBuffer()->layout.href()->retrieveAnchor(po->line,
                                                                      po->pos);
    }
    if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem()) {
      an = CurrentTab->currentBuffer()->layout.formitem()->retrieveAnchor(
          po->line, po->pos);
    }
    hseq++;
  } while (an == nullptr);

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
}

/* go to the last anchor */
// LINK_END
//"Move to the last hyperlink"
void lastA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  if (hl->size() == 0)
    return;

  if (prec_num >= (int)hl->size())
    hseq = 0;
  else if (prec_num > 0)
    hseq = hl->size() - prec_num;
  else
    hseq = hl->size() - 1;
  do {
    if (hseq < 0)
      return;
    po = &hl->marks[hseq];
    if (CurrentTab->currentBuffer()->layout.href()) {
      an = CurrentTab->currentBuffer()->layout.href()->retrieveAnchor(po->line,
                                                                      po->pos);
    }
    if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem()) {
      an = CurrentTab->currentBuffer()->layout.formitem()->retrieveAnchor(
          po->line, po->pos);
    }
    hseq--;
  } while (an == nullptr);

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
}

/* go to the nth anchor */
// LINK_N
//"Go to the nth link"
void nthA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();

  int n = App::instance().searchKeyNum();
  if (n < 0 || n > (int)hl->size()) {
    return;
  }

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  if (!hl || hl->size() == 0)
    return;

  auto po = &hl->marks[n - 1];

  Anchor *an = nullptr;
  if (CurrentTab->currentBuffer()->layout.href()) {
    an = CurrentTab->currentBuffer()->layout.href()->retrieveAnchor(po->line,
                                                                    po->pos);
  }
  if (an == nullptr && CurrentTab->currentBuffer()->layout.formitem()) {
    an = CurrentTab->currentBuffer()->layout.formitem()->retrieveAnchor(
        po->line, po->pos);
  }
  if (an == nullptr)
    return;

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
}

/* go to the next anchor */
// NEXT_LINK
//"Move to the next hyperlink"
void nextA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(false, baseUr, n);
}

/* go to the previous anchor */
// PREV_LINK
//"Move to the previous hyperlink"
void prevA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(false, baseUr, n);
}

/* go to the next visited anchor */
// NEXT_VISITED
//"Move to the next visited hyperlink"
void nextVA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(true, baseUr, n);
}

/* go to the previous visited anchor */
// PREV_VISITED
//"Move to the previous visited hyperlink"
void prevVA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(true, baseUr, n);
}

/* go to the next left anchor */
// NEXT_LEFT
//"Move left to the next hyperlink"
void nextL() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, 0, n);
}

/* go to the next left-up anchor */
// NEXT_LEFT_UP
//"Move left or upward to the next hyperlink"
void nextLU() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, -1, n);
}

/* go to the next right anchor */
// NEXT_RIGHT
//"Move right to the next hyperlink"
void nextR() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 0, n);
}

/* go to the next right-down anchor */
// NEXT_RIGHT_DOWN
//"Move right or downward to the next hyperlink"
void nextRD() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 1, n);
}

/* go to the next downward anchor */
// NEXT_DOWN
//"Move downward to the next hyperlink"
void nextD() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextY(1, n);
}

/* go to the next upward anchor */
// NEXT_UP
//"Move upward to the next hyperlink"
void nextU() {
  int n = App::instance().searchKeyNum();
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
  App::instance().invalidate();
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
  App::instance().invalidate();
}

/* delete current buffer and back to the previous buffer */
// BACK
//"Close current buffer and return to the one below in stack"
void backBf() {
  // Buffer *buf = CurrentTab->currentBuffer()->linkBuffer[LB_N_FRAME];

  if (!checkBackBuffer(CurrentTab->currentBuffer())) {
    if (close_tab_back && App::instance().nTab() >= 1) {
      App::instance().deleteTab(CurrentTab);
      App::instance().invalidate();
    } else
      App::instance().disp_message("Can't go back...");
    return;
  }

  CurrentTab->deleteBuffer(CurrentTab->currentBuffer());

  App::instance().invalidate();
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
void goURL() { CurrentTab->goURL0("Goto URL: ", false); }

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
    CurrentTab->cmd_loadURL(url, {}, {}, nullptr);
    if (CurrentTab->currentBuffer() != cur_buf) /* success */
      pushHashHist(
          URLHist,
          CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
  }
}

// GOTO_RELATIVE
//"Go to relative address"
void gorURL() { CurrentTab->goURL0("Goto relative URL: ", true); }

/* load bookmark */
// BOOKMARK VIEW_BOOKMARK
//"View bookmarks"
void ldBmark() {
  CurrentTab->cmd_loadURL(BookmarkFile, {}, {.no_referer = true}, nullptr);
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
  CurrentTab->cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, {},
                          {.no_referer = true}, request);
}

/* option setting */
// OPTIONS
//"Display options setting panel"
void ldOpt() { CurrentTab->pushBuffer(load_option_panel()); }

/* set an option */
// SET_OPTION
//"Set option"
void setOpt() {
  const char *opt;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  opt = App::instance().searchKeyData();
  if (opt == nullptr || *opt == '\0' || strchr(opt, '=') == nullptr) {
    if (opt != nullptr && *opt != '\0') {
      auto v = get_param_option(opt);
      opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
    }
    // opt = inputStrHist("Set option: ", opt, TextHist);
    if (opt == nullptr || *opt == '\0') {
      App::instance().invalidate();
      return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
  App::instance().invalidate();
}

/* error message list */
// MSGS
//"Display error messages"
void msgs() {
  auto buf = App::instance().message_list_panel();
  CurrentTab->pushBuffer(buf);
}

/* page info */
// INFO
//"Display information about the current document"
void pginfo() {
  auto buf = page_info_panel(CurrentTab->currentBuffer());
  CurrentTab->pushBuffer(buf);
}

/* link,anchor,image list */
// LIST
//"Show all URLs referenced"
void linkLst() {
  if (auto buf = link_list_panel(CurrentTab->currentBuffer())) {
    CurrentTab->pushBuffer(buf);
  }
}

/* cookie list */
// COOKIE
//"View cookie list"
void cooLst() {
  if (auto buf = cookie_list_panel()) {
    CurrentTab->pushBuffer(buf);
  }
}

/* History page */
// HISTORY
//"Show browsing history"
void ldHist() {
  if (auto buf = historyBuffer(URLHist)) {
    CurrentTab->pushBuffer(buf);
  }
}

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

#ifdef _MSC_VER
#else
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  file = App::instance().searchKeyData();
  if (file == nullptr || *file == '\0') {
    // qfile = inputLineHist("Save buffer to: ", nullptr, IN_COMMAND, SaveHist);
    if (qfile == nullptr || *qfile == '\0') {
      App::instance().invalidate();
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
      App::instance().invalidate();
      return;
    }
    f = fopen(file, "w");
    is_pipe = false;
  }
  if (f == nullptr) {
    char *emsg = Sprintf("Can't open %s", file)->ptr;
    App::instance().disp_err_message(emsg);
    return;
  }
  saveBuffer(CurrentTab->currentBuffer(), f, true);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
  App::instance().invalidate();
#endif
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
  App::instance().invalidate();
}

/* peek URL */
// PEEK_LINK
//"Show target address"
void peekURL() { App::instance()._peekURL(0); }

/* peek URL of image */
// PEEK_IMG
//"Show image address"
void peekIMG() { App::instance()._peekURL(1); }

// PEEK
//"Show current address"
void curURL() {
  auto url = App::instance().currentUrl();
  App::instance().disp_message(url.c_str());
}

/* view HTML source */
// SOURCE VIEW
//"Toggle between HTML shown or processed"
void vwSrc() {

  if (CurrentTab->currentBuffer()->res->type.empty()) {
    return;
  }

  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    return;
  }

  auto buf = CurrentTab->currentBuffer()->sourceBuffer();
  CurrentTab->pushBuffer(buf);
}

/* reload */
// RELOAD
//"Load current document anew"
void reload() {

  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't reload stdin");
    return;
  }

  auto sbuf = Buffer::create(0);
  *sbuf = *CurrentTab->currentBuffer();

  bool multipart = false;
  FormList *request;
  if (CurrentTab->currentBuffer()->layout.form_submit) {
    request = CurrentTab->currentBuffer()->layout.form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      multipart = true;
      CurrentTab->currentBuffer()
          ->layout.form_submit->query_from_followform_multipart();
      struct stat st;
      stat(request->body, &st);
      request->length = st.st_size;
    }
  } else {
    request = nullptr;
  }

  auto url = Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str());
  App::instance().message("Reloading...", 0, 0);
  DefaultType = Strnew(CurrentTab->currentBuffer()->res->type)->ptr;

  auto res = loadGeneralFile(url->ptr, {},
                             {.no_referer = true, .no_cache = true}, request);
  DefaultType = nullptr;

  if (multipart) {
    unlink(request->body);
  }
  if (!res) {
    App::instance().disp_err_message("Can't reload...");
    return;
  }

  auto buf = Buffer::create(res);
  // if (buf == NO_BUFFER) {
  //   App::instance().invalidate(CurrentTab->currentBuffer(), B_NORMAL);
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
  App::instance().invalidate();
}

/* reshape */
// RESHAPE
//"Re-render document"
void reshape() {
  CurrentTab->currentBuffer()->layout.need_reshape = true;
  reshapeBuffer(CurrentTab->currentBuffer(),
                App::instance().INIT_BUFFER_WIDTH());
  App::instance().invalidate();
}

// MARK_URL
//"Turn URL-like strings into hyperlinks"
void chkURL() {
  chkURLBuffer(CurrentTab->currentBuffer());
  App::instance().invalidate();
}

// MARK_WORD
//"Turn current word into hyperlink"
void chkWORD() {
  int spos, epos;
  auto p = CurrentTab->currentBuffer()->layout.getCurWord(&spos, &epos);
  if (p.empty())
    return;
  CurrentTab->currentBuffer()->layout.reAnchorWord(
      CurrentTab->currentBuffer()->layout.currentLine, spos, epos);
  App::instance().invalidate();
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
    App::instance().disp_err_message("Can't browse stdin");
    return;
  }
  invoke_browser(CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
}

// EXTERN_LINK
//"Display target using an external browser"
void linkbrz() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    return;
  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentAnchor();
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

  App::instance().disp_message(tmp->ptr);
}

// VERSION
//"Display the version of w3m"
void dispVer() {
  App::instance().disp_message(Sprintf("w3m version %s", w3m_version)->ptr);
}

// WRAP_TOGGLE
//"Toggle wrapping mode in searches"
void wrapToggle() {
  if (WrapSearch) {
    WrapSearch = false;
    App::instance().disp_message("Wrap search off");
  } else {
    WrapSearch = true;
    App::instance().disp_message("Wrap search on");
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
void execCmd() { App::instance().doCmd(); }

// ALARM
//"Set alarm"
void setAlarm() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == nullptr) {
      App::instance().invalidate();
      return;
    }
  }

  FuncId cmd = (FuncId)-1;
  int sec = 0;
  if (*data != '\0') {
    sec = atoi(getWord(&data));
    if (sec > 0) {
      cmd = App::instance().getFuncList(getWord(&data));
    }
  }

  data = getQWord(&data);

  App::instance().task(sec, cmd, data);
}

// REINIT
//"Reload configuration file"
void reinit() {
  auto resource = App::instance().searchKeyData();

  if (resource == nullptr) {
    init_rc();
    sync_with_option();
    initCookie();
    App::instance().invalidate();
    return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
    App::instance().invalidate();
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

  App::instance().disp_err_message(
      Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

// DEFINE_KEY
//"Define a binding between a key stroke combination and a command"
void defKey() {
  const char *data;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data == nullptr || *data == '\0') {
      App::instance().invalidate();
      return;
    }
  }
  App::instance().setKeymap(allocStr(data, -1), -1, true);
  App::instance().invalidate();
}

// NEW_TAB
//"Open a new tab (with current document)"
void newT() {
  App::instance().newTab();
  App::instance().invalidate();
}

// CLOSE_TAB
//"Close tab"
void closeT() {

  if (App::instance().nTab() <= 1)
    return;

  TabBuffer *tab;
  if (prec_num)
    tab = App::instance().numTab(PREC_NUM);
  else
    tab = CurrentTab;
  if (tab)
    App::instance().deleteTab(tab);
  App::instance().invalidate();
}

// NEXT_TAB
//"Switch to the next tab"
void nextT() {
  App::instance().nextTab();
  App::instance().invalidate();
}

// PREV_TAB
//"Switch to the previous tab"
void prevT() {
  App::instance().prevTab();
  App::instance().invalidate();
}

// TAB_LINK
//"Follow current hyperlink in a new tab"
void tabA() {
  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentAnchor();
  if (!a) {
    return;
  }

  App::instance().newTab();
  auto buf = CurrentTab->currentBuffer();

  CurrentTab->followAnchor(false);

  if (buf != CurrentTab->currentBuffer())
    CurrentTab->deleteBuffer(buf);
  else
    App::instance().deleteTab(CurrentTab);
  App::instance().invalidate();
}

// TAB_GOTO
//"Open specified document in a new tab"
void tabURL() {
  App::instance().newTab();

  auto buf = CurrentTab->currentBuffer();
  CurrentTab->goURL0("Goto URL on new tab: ", false);
  if (buf != CurrentTab->currentBuffer())
    CurrentTab->deleteBuffer(buf);
  else
    App::instance().deleteTab(CurrentTab);
  App::instance().invalidate();
}

// TAB_GOTO_RELATIVE
//"Open relative address in a new tab"
void tabrURL() {
  App::instance().newTab();

  auto buf = CurrentTab->currentBuffer();
  CurrentTab->goURL0("Goto relative URL on new tab: ", true);
  if (buf != CurrentTab->currentBuffer())
    CurrentTab->deleteBuffer(buf);
  else
    App::instance().deleteTab(CurrentTab);
  App::instance().invalidate();
}

// TAB_RIGHT
//"Move right along the tab bar"
void tabR() { App::instance().tabRight(); }

// TAB_LEFT
//"Move left along the tab bar"
void tabL() { App::instance().tabLeft(); }

/* download panel */
// DOWNLOAD_LIST
//"Display downloads panel"
void ldDL() {
  int replace = false, new_tab = false;

  if (!FirstDL) {
    if (replace) {
      if (CurrentTab->currentBuffer() == CurrentTab->firstBuffer &&
          CurrentTab->currentBuffer()->backBuffer == nullptr) {
        if (App::instance().nTab() > 1)
          App::instance().deleteTab(CurrentTab);
      } else
        CurrentTab->deleteBuffer(CurrentTab->currentBuffer());
      App::instance().invalidate();
    }
    return;
  }

  auto reload = checkDownloadList();
  auto buf = DownloadListBuffer();
  if (!buf) {
    App::instance().invalidate();
    return;
  }
  if (replace) {
    buf->layout.COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
    buf->layout.restorePosition(CurrentTab->currentBuffer()->layout);
  }
  if (!replace && open_tab_dl_list) {
    App::instance().newTab();
    new_tab = true;
  }
  CurrentTab->pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf();
  if (reload) {
    // CurrentTab->currentBuffer()->layout.event =
    //     setAlarmEvent(CurrentTab->currentBuffer()->layout.event, 1,
    //     AL_IMPLICIT,
    //                   FUNCNAME_reload, nullptr);
  }
  App::instance().invalidate();
}

// UNDO
//"Cancel the last cursor movement"
void undoPos() {
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    return;
  CurrentTab->currentBuffer()->layout.undoPos(PREC_NUM);
}

// REDO
//"Cancel the last undo"
void redoPos() {
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    return;
  CurrentTab->currentBuffer()->layout.redoPos(PREC_NUM);
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
  App::instance().invalidate();
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
  App::instance().invalidate();
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
  App::instance().invalidate();
}
