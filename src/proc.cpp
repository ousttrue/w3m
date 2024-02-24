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
#include "tabbuffer.h"
#include "buffer.h"
#include <sys/stat.h>

// NOTHING nullptr @ @ @
//"Do nothing"
FuncCoroutine<void> nulcmd() { /* do nothing */
  co_return;
}

/* Move page forward */
// NEXT_PAGE
//"Scroll down one page"
FuncCoroutine<void> pgFore() {
  CurrentTab->currentBuffer()->layout.nscroll(
      prec_num ? App::instance().searchKeyNum()
               : App::instance().searchKeyNum() *
                     (CurrentTab->currentBuffer()->layout.LINES - 1));
  co_return;
}

/* Move page backward */
// PREV_PAGE
//"Scroll up one page"
FuncCoroutine<void> pgBack() {
  CurrentTab->currentBuffer()->layout.nscroll(
      -(prec_num ? App::instance().searchKeyNum()
                 : App::instance().searchKeyNum() *
                       (CurrentTab->currentBuffer()->layout.LINES - 1)));
  co_return;
}

/* Move half page forward */
// NEXT_HALF_PAGE
//"Scroll down half a page"
FuncCoroutine<void> hpgFore() {
  CurrentTab->currentBuffer()->layout.nscroll(
      App::instance().searchKeyNum() *
      (CurrentTab->currentBuffer()->layout.LINES / 2 - 1));
  co_return;
}

/* Move half page backward */
// PREV_HALF_PAGE
//"Scroll up half a page"
FuncCoroutine<void> hpgBack() {
  CurrentTab->currentBuffer()->layout.nscroll(
      -App::instance().searchKeyNum() *
      (CurrentTab->currentBuffer()->layout.LINES / 2 - 1));
  co_return;
}

/* 1 line up */
// UP
//"Scroll the screen up one line"
FuncCoroutine<void> lup1() {
  CurrentTab->currentBuffer()->layout.nscroll(App::instance().searchKeyNum());
  co_return;
}

/* 1 line down */
// DOWN
//"Scroll the screen down one line"
FuncCoroutine<void> ldown1() {
  CurrentTab->currentBuffer()->layout.nscroll(-App::instance().searchKeyNum());
  co_return;
}

/* move cursor position to the center of screen */
// CENTER_V
//"Center on cursor line"
FuncCoroutine<void> ctrCsrV() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> ctrCsrH() {
  int offsetx;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> rdrwSc() {
  // clear();
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
  co_return;
}

// SEARCH SEARCH_FORE WHEREIS
//"Search forward"
FuncCoroutine<void> srchfor() {
  srch(forwardSearch, "Forward: ");
  co_return;
}

// ISEARCH
//"Incremental search forward"
FuncCoroutine<void> isrchfor() {
  isrch(forwardSearch, "I-search: ");
  co_return;
}

/* Search regular expression backward */

// SEARCH_BACK
//"Search backward"
FuncCoroutine<void> srchbak() {
  srch(backwardSearch, "Backward: ");
  co_return;
}

// ISEARCH_BACK
//"Incremental search backward"
FuncCoroutine<void> isrchbak() {
  isrch(backwardSearch, "I-search backward: ");
  co_return;
}

/* Search next matching */
// SEARCH_NEXT
//"Continue search forward"
FuncCoroutine<void> srchnxt() {
  srch_nxtprv(0);
  co_return;
}

/* Search previous matching */
// SEARCH_PREV
//"Continue search backward"
FuncCoroutine<void> srchprv() {
  srch_nxtprv(1);
  co_return;
}

/* Shift screen left */
// SHIFT_LEFT
//"Shift screen left"
FuncCoroutine<void> shiftl() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> shiftr() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> col1R() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, column, n = App::instance().searchKeyNum();

  if (l == nullptr)
    co_return;
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
FuncCoroutine<void> col1L() {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout.currentLine;
  int j, n = App::instance().searchKeyNum();

  if (l == nullptr)
    co_return;
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
FuncCoroutine<void> setEnv() {
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
      co_return;
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
FuncCoroutine<void> pipeBuf() { co_return; }

/* Execute shell command and read output ac pipe. */
// PIPE_SHELL
//"Execute shell command and display output"
FuncCoroutine<void> pipesh() { co_return; }

/* Execute shell command and load entire output to buffer */
// READ_SHELL
//"Execute shell command and display output"
FuncCoroutine<void> readsh() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = App::instance().searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd == nullptr || *cmd == '\0') {
    App::instance().invalidate();
    co_return;
  }
  // MySignalHandler prevtrap = {};
  // prevtrap = mySignal(SIGINT, intTrap);
  // crmode();
  auto buf = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  // term_raw();
  if (buf == nullptr) {
    App::instance().disp_message("Execution failed");
    co_return;
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
FuncCoroutine<void> execsh() {
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
  co_return;
}

/* Load file */
// LOAD
//"Open local file in a new buffer"
FuncCoroutine<void> ldfile() {
  auto fn = App::instance().searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn == nullptr || *fn == '\0') {
    App::instance().invalidate();
    co_return;
  }
  cmd_loadfile(fn);
}

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

/* Load help file */
// HELP
//"Show help panel"
FuncCoroutine<void> ldhelp() {
  auto lang = AcceptLang;
  auto n = strcspn(lang, ";, \t");
  auto tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  CurrentTab->cmd_loadURL(tmp->ptr, {}, {.no_referer = true}, nullptr);
  co_return;
}

// MOVE_LEFT
//"Cursor left"
FuncCoroutine<void> movL() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
  co_return;
}

// MOVE_LEFT1
//"Cursor left. With edge touched, slide"
FuncCoroutine<void> movL1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movL(1, m);
  co_return;
}

// MOVE_DOWN
//"Cursor down"
FuncCoroutine<void> movD() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
  co_return;
}

// MOVE_DOWN1
//"Cursor down. With edge touched, slide"
FuncCoroutine<void> movD1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movD(1, m);
  co_return;
}

// MOVE_UP
//"Cursor up"
FuncCoroutine<void> movU() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(
      (CurrentTab->currentBuffer()->layout.LINES + 1) / 2, m);
  co_return;
}

// MOVE_UP1
//"Cursor up. With edge touched, slide"
FuncCoroutine<void> movU1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movU(1, m);
  co_return;
}

// MOVE_RIGHT
//"Cursor right"
FuncCoroutine<void> movR() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(
      CurrentTab->currentBuffer()->layout.COLS / 2, m);
  co_return;
}

// MOVE_RIGHT1
//"Cursor right. With edge touched, slide"
FuncCoroutine<void> movR1() {
  int m = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout._movR(1, m);
  co_return;
}

// PREV_WORD
//"Move to the previous word"
FuncCoroutine<void> movLW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = App::instance().searchKeyNum();

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;

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
FuncCoroutine<void> movRW() {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = App::instance().searchKeyNum();

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;

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
FuncCoroutine<void> quitfm() {
  App::instance().exit(0);
  co_return;
}

/* Question and Quit */
// QUIT
//"Quit with confirmation request"
FuncCoroutine<void> qquitfm() {
  App::instance().exit(0);
  co_return;
}

/* Select buffer */
// SELECT
//"Display buffer-stack panel"
FuncCoroutine<void> selBuf() {

  // auto ok = false;
  // do {
  //   char cmd;
  //   auto buf = selectBuffer(CurrentTab->firstBuffer,
  //                           CurrentTab->currentBuffer(), &cmd);
  //   ok = CurrentTab->select(cmd, buf);
  // } while (!ok);
  //
  // App::instance().invalidate();
  co_return;
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
// INTERRUPT SUSPEND
//"Suspend w3m to background"
FuncCoroutine<void> susp() {
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
  co_return;
}

// GOTO_LINE
//"Go to the specified line"
FuncCoroutine<void> goLine() {

  auto str = App::instance().searchKeyData();
  if (prec_num)
    CurrentTab->currentBuffer()->layout._goLine("^", prec_num);
  else if (str)
    CurrentTab->currentBuffer()->layout._goLine(str, prec_num);
  else {
    // _goLine(inputStr("Goto line: ", ""));
  }
  co_return;
}

// BEGIN
//"Go to the first line"
FuncCoroutine<void> goLineF() {
  CurrentTab->currentBuffer()->layout._goLine("^", prec_num);
  co_return;
}

// END
//"Go to the last line"
FuncCoroutine<void> goLineL() {
  CurrentTab->currentBuffer()->layout._goLine("$", prec_num);
  co_return;
}

/* Go to the beginning of the line */
// LINE_BEGIN
//"Go to the beginning of the line"
FuncCoroutine<void> linbeg() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> linend() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
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
FuncCoroutine<void> editBf() {
  auto fn = CurrentTab->currentBuffer()->res->filename;

  if (fn.empty() || /* Behaving as a pager */
      (CurrentTab->currentBuffer()->res->type.empty() &&
       CurrentTab->currentBuffer()->res->edit == nullptr) || /* Reading shell */
      CurrentTab->currentBuffer()->res->currentURL.schema != SCM_LOCAL ||
      CurrentTab->currentBuffer()->res->currentURL.file ==
          "-" /* file is std input  */
  ) {
    App::instance().disp_err_message("Can't edit other than local file");
    co_return;
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
FuncCoroutine<void> editScr() {
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == nullptr) {
    App::instance().disp_err_message(
        Sprintf("Can't open %s", tmpf.c_str())->ptr);
    co_return;
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
FuncCoroutine<void> followA() {

  CurrentTab->followAnchor();

  App::instance().invalidate();
  co_return;
}

/* view inline image */
// VIEW_IMAGE
//"Display image in viewer"
FuncCoroutine<void> followI() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;

  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentImg();
  if (a == nullptr)
    co_return;
  App::instance().message(Sprintf("loading %s", a->url)->ptr, 0, 0);

  auto res = loadGeneralFile(
      a->url, CurrentTab->currentBuffer()->res->getBaseURL(), {});
  if (!res) {
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    App::instance().disp_err_message(emsg);
    co_return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
  App::instance().invalidate();
}

/* submit form */
// SUBMIT
//"Submit form"
FuncCoroutine<void> submitForm() {
  if (auto f = CurrentTab->currentBuffer()->layout.retrieveCurrentForm()) {
    auto buf = CurrentTab->currentBuffer()->followForm(f, true);
    if (buf) {
      App::instance().pushBuffer(buf, f->target);
    }
  }
  co_return;
}

/* go to the top anchor */
// LINK_BEGIN
//"Move to the first hyperlink"
FuncCoroutine<void> topA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  if (hl->size() == 0)
    co_return;

  if (prec_num > (int)hl->size())
    hseq = hl->size() - 1;
  else if (prec_num > 0)
    hseq = prec_num - 1;
  do {
    if (hseq >= (int)hl->size())
      co_return;
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
FuncCoroutine<void> lastA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  if (hl->size() == 0)
    co_return;

  if (prec_num >= (int)hl->size())
    hseq = 0;
  else if (prec_num > 0)
    hseq = hl->size() - prec_num;
  else
    hseq = hl->size() - 1;
  do {
    if (hseq < 0)
      co_return;
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
FuncCoroutine<void> nthA() {
  auto hl = CurrentTab->currentBuffer()->layout.hmarklist();

  int n = App::instance().searchKeyNum();
  if (n < 0 || n > (int)hl->size()) {
    co_return;
  }

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  if (!hl || hl->size() == 0)
    co_return;

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
    co_return;

  CurrentTab->currentBuffer()->layout.gotoLine(po->line);
  CurrentTab->currentBuffer()->layout.pos = po->pos;
  CurrentTab->currentBuffer()->layout.arrangeCursor();
  App::instance().invalidate();
}

/* go to the next anchor */
// NEXT_LINK
//"Move to the next hyperlink"
FuncCoroutine<void> nextA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(false, baseUr, n);
  co_return;
}

/* go to the previous anchor */
// PREV_LINK
//"Move to the previous hyperlink"
FuncCoroutine<void> prevA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(false, baseUr, n);
  co_return;
}

/* go to the next visited anchor */
// NEXT_VISITED
//"Move to the next visited hyperlink"
FuncCoroutine<void> nextVA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._nextA(true, baseUr, n);
  co_return;
}

/* go to the previous visited anchor */
// PREV_VISITED
//"Move to the previous visited hyperlink"
FuncCoroutine<void> prevVA() {
  int n = App::instance().searchKeyNum();
  auto baseUr = CurrentTab->currentBuffer()->res->getBaseURL();
  CurrentTab->currentBuffer()->layout._prevA(true, baseUr, n);
  co_return;
}

/* go to the next left anchor */
// NEXT_LEFT
//"Move left to the next hyperlink"
FuncCoroutine<void> nextL() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, 0, n);
  co_return;
}

/* go to the next left-up anchor */
// NEXT_LEFT_UP
//"Move left or upward to the next hyperlink"
FuncCoroutine<void> nextLU() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(-1, -1, n);
  co_return;
}

/* go to the next right anchor */
// NEXT_RIGHT
//"Move right to the next hyperlink"
FuncCoroutine<void> nextR() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 0, n);
  co_return;
}

/* go to the next right-down anchor */
// NEXT_RIGHT_DOWN
//"Move right or downward to the next hyperlink"
FuncCoroutine<void> nextRD() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextX(1, 1, n);
  co_return;
}

/* go to the next downward anchor */
// NEXT_DOWN
//"Move downward to the next hyperlink"
FuncCoroutine<void> nextD() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextY(1, n);
  co_return;
}

/* go to the next upward anchor */
// NEXT_UP
//"Move upward to the next hyperlink"
FuncCoroutine<void> nextU() {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout.nextY(-1, n);
  co_return;
}

/* go to the next bufferr */
// NEXT
//"Switch to the next buffer"
FuncCoroutine<void> nextBf() {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf =
        forwardBuffer(CurrentTab->firstBuffer, CurrentTab->currentBuffer());
    if (!buf) {
      if (i == 0)
        co_return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
  App::instance().invalidate();
}

/* go to the previous bufferr */
// PREV
//"Switch to the previous buffer"
FuncCoroutine<void> prevBf() {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf = CurrentTab->currentBuffer()->backBuffer;
    if (!buf) {
      if (i == 0)
        co_return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
  App::instance().invalidate();
}

/* delete current buffer and back to the previous buffer */
// BACK
//"Close current buffer and return to the one below in stack"
FuncCoroutine<void> backBf() {
  // Buffer *buf = CurrentTab->currentBuffer()->linkBuffer[LB_N_FRAME];

  if (!checkBackBuffer(CurrentTab->currentBuffer())) {
    if (close_tab_back && App::instance().nTab() >= 1) {
      App::instance().deleteTab(CurrentTab);
      App::instance().invalidate();
    } else
      App::instance().disp_message("Can't go back...");
    co_return;
  }

  CurrentTab->deleteBuffer(CurrentTab->currentBuffer());

  App::instance().invalidate();
}

// DELETE_PREVBUF
//"Delete previous buffer (mainly for local CGI-scripts)"
FuncCoroutine<void> deletePrevBuf() {
  auto buf = CurrentTab->currentBuffer()->backBuffer;
  if (buf) {
    CurrentTab->deleteBuffer(buf);
  }
  co_return;
}

// GOTO
//"Open specified document in a new buffer"
FuncCoroutine<void> goURL() {
  CurrentTab->goURL0("Goto URL: ", false);
  co_return;
}

// GOTO_HOME
//"Open home page in a new buffer"
FuncCoroutine<void> goHome() {
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
  co_return;
}

// GOTO_RELATIVE
//"Go to relative address"
FuncCoroutine<void> gorURL() {
  CurrentTab->goURL0("Goto relative URL: ", true);
  co_return;
}

/* load bookmark */
// BOOKMARK VIEW_BOOKMARK
//"View bookmarks"
FuncCoroutine<void> ldBmark() {
  CurrentTab->cmd_loadURL(BookmarkFile, {}, {.no_referer = true}, nullptr);
  co_return;
}

/* Add current to bookmark */
// ADD_BOOKMARK
//"Add current page to bookmarks"
FuncCoroutine<void> adBmark() {
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
  co_return;
}

/* option setting */
// OPTIONS
//"Display options setting panel"
FuncCoroutine<void> ldOpt() {
  CurrentTab->pushBuffer(load_option_panel());
  co_return;
}

/* set an option */
// SET_OPTION
//"Set option"
FuncCoroutine<void> setOpt() {
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
      co_return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
  App::instance().invalidate();
}

/* error message list */
// MSGS
//"Display error messages"
FuncCoroutine<void> msgs() {
  auto buf = App::instance().message_list_panel();
  CurrentTab->pushBuffer(buf);
  co_return;
}

/* page info */
// INFO
//"Display information about the current document"
FuncCoroutine<void> pginfo() {
  auto buf = page_info_panel(CurrentTab->currentBuffer());
  CurrentTab->pushBuffer(buf);
  co_return;
}

/* link,anchor,image list */
// LIST
//"Show all URLs referenced"
FuncCoroutine<void> linkLst() {
  if (auto buf = link_list_panel(CurrentTab->currentBuffer())) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* cookie list */
// COOKIE
//"View cookie list"
FuncCoroutine<void> cooLst() {
  if (auto buf = cookie_list_panel()) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* History page */
// HISTORY
//"Show browsing history"
FuncCoroutine<void> ldHist() {
  if (auto buf = historyBuffer(URLHist)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* download HREF link */
// SAVE_LINK
//"Save hyperlink target"
FuncCoroutine<void> svA() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followA();
  // do_download = false;
  co_return;
}

/* download IMG link */
// SAVE_IMAGE
//"Save inline image"
FuncCoroutine<void> svI() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followI();
  // do_download = false;
  co_return;
}

/* save buffer */
// PRINT SAVE_SCREEN
//"Save rendered document"
FuncCoroutine<void> svBuf() {
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
      co_return;
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
      co_return;
    }
    f = fopen(file, "w");
    is_pipe = false;
  }
  if (f == nullptr) {
    char *emsg = Sprintf("Can't open %s", file)->ptr;
    App::instance().disp_err_message(emsg);
    co_return;
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
FuncCoroutine<void> svSrc() {
  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    co_return;
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
FuncCoroutine<void> peekURL() {
  App::instance()._peekURL(0);
  co_return;
}

/* peek URL of image */
// PEEK_IMG
//"Show image address"
FuncCoroutine<void> peekIMG() {
  App::instance()._peekURL(1);
  co_return;
}

// PEEK
//"Show current address"
FuncCoroutine<void> curURL() {
  auto url = App::instance().currentUrl();
  App::instance().disp_message(url.c_str());
  co_return;
}

/* view HTML source */
// SOURCE VIEW
//"Toggle between HTML shown or processed"
FuncCoroutine<void> vwSrc() {

  if (CurrentTab->currentBuffer()->res->type.empty()) {
    co_return;
  }

  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    co_return;
  }

  auto buf = CurrentTab->currentBuffer()->sourceBuffer();
  CurrentTab->pushBuffer(buf);
}

/* reload */
// RELOAD
//"Load current document anew"
FuncCoroutine<void> reload() {

  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't reload stdin");
    co_return;
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
    co_return;
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
FuncCoroutine<void> reshape() {
  CurrentTab->currentBuffer()->layout.need_reshape = true;
  reshapeBuffer(CurrentTab->currentBuffer(),
                App::instance().INIT_BUFFER_WIDTH());
  App::instance().invalidate();
  co_return;
}

// MARK_URL
//"Turn URL-like strings into hyperlinks"
FuncCoroutine<void> chkURL() {
  chkURLBuffer(CurrentTab->currentBuffer());
  App::instance().invalidate();
  co_return;
}

// MARK_WORD
//"Turn current word into hyperlink"
FuncCoroutine<void> chkWORD() {
  int spos, epos;
  auto p = CurrentTab->currentBuffer()->layout.getCurWord(&spos, &epos);
  if (p.empty())
    co_return;
  CurrentTab->currentBuffer()->layout.reAnchorWord(
      CurrentTab->currentBuffer()->layout.currentLine, spos, epos);
  App::instance().invalidate();
}

/* render frames */
// FRAME
//"Toggle rendering HTML frames"
FuncCoroutine<void> rFrame() { co_return; }

// EXTERN
//"Display using an external browser"
FuncCoroutine<void> extbrz() {
  if (CurrentTab->currentBuffer()->res->currentURL.schema == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't browse stdin");
    co_return;
  }
  invoke_browser(CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
}

// EXTERN_LINK
//"Display target using an external browser"
FuncCoroutine<void> linkbrz() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentAnchor();
  if (a == nullptr)
    co_return;
  auto pu = urlParse(a->url, CurrentTab->currentBuffer()->res->getBaseURL());
  invoke_browser(pu.to_Str().c_str());
}

/* show current line number and number of lines in the entire document */
// LINE_INFO
//"Display current position in document"
FuncCoroutine<void> curlno() {
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
  co_return;
}

// VERSION
//"Display the version of w3m"
FuncCoroutine<void> dispVer() {
  App::instance().disp_message(Sprintf("w3m version %s", w3m_version)->ptr);
  co_return;
}

// WRAP_TOGGLE
//"Toggle wrapping mode in searches"
FuncCoroutine<void> wrapToggle() {
  if (WrapSearch) {
    WrapSearch = false;
    App::instance().disp_message("Wrap search off");
  } else {
    WrapSearch = true;
    App::instance().disp_message("Wrap search on");
  }
  co_return;
}

// DICT_WORD
//"Execute dictionary command (see README.dict)"
FuncCoroutine<void> dictword() {
  // execdict(inputStr("(dictionary)!", ""));
  co_return;
}

// DICT_WORD_AT
//"Execute dictionary command for word at cursor"
FuncCoroutine<void> dictwordat() {
  auto word = CurrentTab->currentBuffer()->layout.getCurWord();
  execdict(word.c_str());
  co_return;
}

// COMMAND
//"Invoke w3m function(s)"
FuncCoroutine<void> execCmd() {
  App::instance().doCmd();
  co_return;
}

// ALARM
//"Set alarm"
FuncCoroutine<void> setAlarm() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == nullptr) {
      App::instance().invalidate();
      co_return;
    }
  }

  std::string cmd = "";
  int sec = 0;
  if (*data != '\0') {
    sec = atoi(getWord(&data));
    if (sec > 0) {
      cmd = getWord(&data);
    }
  }

  data = getQWord(&data);

  App::instance().task(sec, cmd, data);
}

// REINIT
//"Reload configuration file"
FuncCoroutine<void> reinit() {
  auto resource = App::instance().searchKeyData();

  if (resource == nullptr) {
    init_rc();
    sync_with_option();
    initCookie();
    App::instance().invalidate();
    co_return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
    App::instance().invalidate();
    co_return;
  }

  if (!strcasecmp(resource, "COOKIE")) {
    initCookie();
    co_return;
  }

  if (!strcasecmp(resource, "KEYMAP")) {
    App::instance().initKeymap(true);
    co_return;
  }

  if (!strcasecmp(resource, "MAILCAP")) {
    initMailcap();
    co_return;
  }

  if (!strcasecmp(resource, "MIMETYPES")) {
    initMimeTypes();
    co_return;
  }

  App::instance().disp_err_message(
      Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

// DEFINE_KEY
//"Define a binding between a key stroke combination and a command"
FuncCoroutine<void> defKey() {
  const char *data;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data == nullptr || *data == '\0') {
      App::instance().invalidate();
      co_return;
    }
  }
  App::instance().setKeymap(allocStr(data, -1), -1, true);
  App::instance().invalidate();
}

// NEW_TAB
//"Open a new tab (with current document)"
FuncCoroutine<void> newT() {
  App::instance().newTab();
  App::instance().invalidate();
  co_return;
}

// CLOSE_TAB
//"Close tab"
FuncCoroutine<void> closeT() {

  if (App::instance().nTab() <= 1)
    co_return;

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
FuncCoroutine<void> nextT() {
  App::instance().nextTab();
  App::instance().invalidate();
  co_return;
}

// PREV_TAB
//"Switch to the previous tab"
FuncCoroutine<void> prevT() {
  App::instance().prevTab();
  App::instance().invalidate();
  co_return;
}

// TAB_LINK
//"Follow current hyperlink in a new tab"
FuncCoroutine<void> tabA() {
  auto a = CurrentTab->currentBuffer()->layout.retrieveCurrentAnchor();
  if (!a) {
    co_return;
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
FuncCoroutine<void> tabURL() {
  App::instance().newTab();

  auto buf = CurrentTab->currentBuffer();
  CurrentTab->goURL0("Goto URL on new tab: ", false);
  if (buf != CurrentTab->currentBuffer())
    CurrentTab->deleteBuffer(buf);
  else
    App::instance().deleteTab(CurrentTab);
  App::instance().invalidate();
  co_return;
}

// TAB_GOTO_RELATIVE
//"Open relative address in a new tab"
FuncCoroutine<void> tabrURL() {
  App::instance().newTab();

  auto buf = CurrentTab->currentBuffer();
  CurrentTab->goURL0("Goto relative URL on new tab: ", true);
  if (buf != CurrentTab->currentBuffer())
    CurrentTab->deleteBuffer(buf);
  else
    App::instance().deleteTab(CurrentTab);
  App::instance().invalidate();
  co_return;
}

// TAB_RIGHT
//"Move right along the tab bar"
FuncCoroutine<void> tabR() {
  App::instance().tabRight();
  co_return;
}

// TAB_LEFT
//"Move left along the tab bar"
FuncCoroutine<void> tabL() {
  App::instance().tabLeft();
  co_return;
}

/* download panel */
// DOWNLOAD_LIST
//"Display downloads panel"
FuncCoroutine<void> ldDL() {
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
    co_return;
  }

  auto reload = checkDownloadList();
  auto buf = DownloadListBuffer();
  if (!buf) {
    App::instance().invalidate();
    co_return;
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
FuncCoroutine<void> undoPos() {
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    co_return;
  CurrentTab->currentBuffer()->layout.undoPos(PREC_NUM);
}

// REDO
//"Cancel the last undo"
FuncCoroutine<void> redoPos() {
  if (!CurrentTab->currentBuffer()->layout.firstLine)
    co_return;
  CurrentTab->currentBuffer()->layout.redoPos(PREC_NUM);
}

// CURSOR_TOP
//"Move cursor to the top of the screen"
FuncCoroutine<void> cursorTop() {
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.lineSkip(
          CurrentTab->currentBuffer()->layout.topLine, 0, false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  App::instance().invalidate();
}

// CURSOR_MIDDLE
//"Move cursor to the middle of the screen"
FuncCoroutine<void> cursorMiddle() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  offsety = (CurrentTab->currentBuffer()->layout.LINES - 1) / 2;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.topLine->currentLineSkip(offsety,
                                                                   false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  App::instance().invalidate();
}

// CURSOR_BOTTOM
//"Move cursor to the bottom of the screen"
FuncCoroutine<void> cursorBottom() {
  int offsety;
  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr)
    co_return;
  offsety = CurrentTab->currentBuffer()->layout.LINES - 1;
  CurrentTab->currentBuffer()->layout.currentLine =
      CurrentTab->currentBuffer()->layout.topLine->currentLineSkip(offsety,
                                                                   false);
  CurrentTab->currentBuffer()->layout.arrangeLine();
  App::instance().invalidate();
}

/* follow HREF link in the buffer */
FuncCoroutine<void> bufferA(void) {
  on_target = false;
  followA();
  on_target = true;
  co_return;
}
