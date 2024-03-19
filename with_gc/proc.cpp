#include "proc.h"
#include "option.h"
#include "ioutil.h"
#include "dict.h"
#include "app.h"
#include "html/form_item.h"
#include "file_util.h"
#include "html/readbuffer.h"
#include "http_response.h"
#include "url_quote.h"
#include "search.h"
#include "mimetypes.h"
#include "downloadlist.h"
#include "linein.h"
#include "cookie.h"
#include "rc.h"
#include "html/form.h"
#include "history.h"
#include "html/anchorlist.h"
#include "etc.h"
#include "mailcap.h"
#include "quote.h"
#include "http_request.h"
#include "http_response.h"
#include "http_session.h"
#include "local_cgi.h"
#include "Str.h"
#include "search.h"
#include "app.h"
#include "myctype.h"
#include "tabbuffer.h"
#include "buffer.h"
#include <sys/stat.h>

// NOTHING nullptr @ @ @
//"Do nothing"
std::shared_ptr<CoroutineState<void>>
nulcmd(const FuncContext &context) { /* do nothing */
  co_return;
}

/* Move page forward */
// NEXT_PAGE
//"Scroll down one page"
std::shared_ptr<CoroutineState<void>> pgFore(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  buf->layout->visual.scrollMoveRow(
      (prec_num ? App::instance().searchKeyNum()
                : App::instance().searchKeyNum() * buf->layout->LINES()));
  co_return;
}

/* Move page backward */
// PREV_PAGE
//"Scroll up one page"
std::shared_ptr<CoroutineState<void>> pgBack(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  buf->layout->visual.scrollMoveRow(
      (-(prec_num ? App::instance().searchKeyNum()
                  : App::instance().searchKeyNum() * buf->layout->LINES())));
  co_return;
}

/* Move half page forward */
// NEXT_HALF_PAGE
//"Scroll down half a page"
std::shared_ptr<CoroutineState<void>> hpgFore(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  buf->layout->visual.scrollMoveRow(
      (App::instance().searchKeyNum() * (buf->layout->LINES() / 2)));
  co_return;
}

/* Move half page backward */
// PREV_HALF_PAGE
//"Scroll up half a page"
std::shared_ptr<CoroutineState<void>> hpgBack(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  buf->layout->visual.scrollMoveRow(
      (-App::instance().searchKeyNum() * (buf->layout->LINES() / 2)));
  co_return;
}

/* 1 line up */
// UP
//"Scroll the screen up one line"
std::shared_ptr<CoroutineState<void>> lup1(const FuncContext &context) {
  CurrentTab->currentBuffer()->layout->visual.scrollMoveRow(
      (App::instance().searchKeyNum()));
  co_return;
}

/* 1 line down */
// DOWN
//"Scroll the screen down one line"
std::shared_ptr<CoroutineState<void>> ldown1(const FuncContext &context) {
  CurrentTab->currentBuffer()->layout->visual.scrollMoveRow(
      (-App::instance().searchKeyNum()));
  co_return;
}

/* move cursor position to the center of screen */
// CENTER_V
//"Center on cursor line"
std::shared_ptr<CoroutineState<void>> ctrCsrV(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = buf->layout->LINES() / 2 - buf->layout->visual.cursor().row;
  if (offsety != 0) {
    buf->layout->visual.scrollMoveRow(-offsety);
  }
}

// CENTER_H
//"Center on cursor column"
std::shared_ptr<CoroutineState<void>> ctrCsrH(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsetx = buf->layout->visual.cursor().col - buf->layout->LINES() / 2;
  if (offsetx != 0) {
    buf->layout->visual.cursorMoveCol(offsetx);
  }
}

/* Redraw screen */
// REDRAW
//"Draw the screen anew"
std::shared_ptr<CoroutineState<void>> rdrwSc(const FuncContext &context) {
  // clear();
  co_return;
}

// SEARCH SEARCH_FORE WHEREIS
//"Search forward"
std::shared_ptr<CoroutineState<void>> srchfor(const FuncContext &context) {
  srch(forwardSearch, "Forward: ");
  co_return;
}

// ISEARCH
//"Incremental search forward"
std::shared_ptr<CoroutineState<void>> isrchfor(const FuncContext &context) {
  isrch(forwardSearch, "I-search: ");
  co_return;
}

/* Search regular expression backward */

// SEARCH_BACK
//"Search backward"
std::shared_ptr<CoroutineState<void>> srchbak(const FuncContext &context) {
  srch(backwardSearch, "Backward: ");
  co_return;
}

// ISEARCH_BACK
//"Incremental search backward"
std::shared_ptr<CoroutineState<void>> isrchbak(const FuncContext &context) {
  isrch(backwardSearch, "I-search backward: ");
  co_return;
}

/* Search next matching */
// SEARCH_NEXT
//"Continue search forward"
std::shared_ptr<CoroutineState<void>> srchnxt(const FuncContext &context) {
  srch_nxtprv(0);
  co_return;
}

/* Search previous matching */
// SEARCH_PREV
//"Continue search backward"
std::shared_ptr<CoroutineState<void>> srchprv(const FuncContext &context) {
  srch_nxtprv(1);
  co_return;
}

/* Shift screen left */
// SHIFT_LEFT
//"Shift screen left"
std::shared_ptr<CoroutineState<void>> shiftl(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;
  int column = buf->layout->visual.cursor().col;
  buf->layout->visual.cursorMoveCol(App::instance().searchKeyNum() *
                                    -buf->layout->COLS());
  buf->layout->visual.cursorMoveCol(-column);
}

/* Shift screen right */
// SHIFT_RIGHT
//"Shift screen right"
std::shared_ptr<CoroutineState<void>> shiftr(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;
  // int column = CurrentTab->currentBuffer()->layout->currentColumn;
  // CurrentTab->currentBuffer()->layout->columnSkip(
  //     App::instance().searchKeyNum() *
  //         (App::instance().contentCols() - 1) -
  //     1);
  // CurrentTab->currentBuffer()->layout->shiftvisualpos(
  //     CurrentTab->currentBuffer()->layout->currentColumn - column);
}

// RIGHT
//"Shift screen one column right"
std::shared_ptr<CoroutineState<void>> col1R(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout->currentLine();
  int j, column, n = App::instance().searchKeyNum();

  if (l == nullptr)
    co_return;
  for (j = 0; j < n; j++) {
    // column = buf->layout->currentColumn;
    // CurrentTab->currentBuffer()->layout->columnSkip(1);
    // if (column == buf->layout->currentColumn)
    //   break;
    // CurrentTab->currentBuffer()->layout->shiftvisualpos(1);
  }
}

// LEFT
//"Shift screen one column left"
std::shared_ptr<CoroutineState<void>> col1L(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  Line *l = buf->layout->currentLine();
  int j, n = App::instance().searchKeyNum();

  if (l == nullptr)
    co_return;
  for (j = 0; j < n; j++) {
    // if (buf->layout->currentColumn == 0)
    //   break;
    // CurrentTab->currentBuffer()->layout->columnSkip(-1);
    // CurrentTab->currentBuffer()->layout->shiftvisualpos(-1);
  }
}

// SETENV
//"Set environment variable"
std::shared_ptr<CoroutineState<void>> setEnv(const FuncContext &context) {
  const char *env;
  const char *var, *value;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  env = App::instance().searchKeyData();
  if (env == nullptr || *env == '\0' || strchr(env, '=') == nullptr) {
    if (env != nullptr && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    // env = inputStrHist("Set environ: ", env, TextHist);
    if (env == nullptr || *env == '\0') {
      co_return;
    }
  }
  if ((value = strchr(env, '=')) != nullptr && value > env) {
    var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
}

// PIPE_BUF
//"Pipe current buffer through a shell command and display output"
std::shared_ptr<CoroutineState<void>> pipeBuf(const FuncContext &context) {
  co_return;
}

/* Execute shell command and read output ac pipe. */
// PIPE_SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>> pipesh(const FuncContext &context) {
  co_return;
}

/* Execute shell command and load entire output to buffer */
// READ_SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>> readsh(const FuncContext &context) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = App::instance().searchKeyData();
  if (cmd == nullptr || *cmd == '\0') {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd == nullptr || *cmd == '\0') {
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
}

/* Execute shell command */
// EXEC_SHELL SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>> execsh(const FuncContext &context) {
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
  co_return;
}

/* Load file */
// LOAD
//"Open local file in a new buffer"
std::shared_ptr<CoroutineState<void>> ldfile(const FuncContext &context) {
  auto fn = App::instance().searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn == nullptr || *fn == '\0') {
    co_return;
  }
  if (auto res =
          loadGeneralFile(file_to_url((char *)fn), {}, {.no_referer = true})) {
    auto buf = Buffer::create(res);
    CurrentTab->pushBuffer(buf);
  } else {
    char *emsg = Sprintf("%s not found", fn)->ptr;
    App::instance().disp_err_message(emsg);
  }
}

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

/* Load help file */
// HELP
//"Show help panel"
std::shared_ptr<CoroutineState<void>> ldhelp(const FuncContext &context) {
  auto lang = AcceptLang;
  auto n = strcspn(lang.c_str(), ";, \t");
  auto tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              form_quote(w3m_version).c_str(),
              form_quote(std::string_view(lang.c_str(), n)).c_str());
  if (auto buf = CurrentTab->currentBuffer()->cmd_loadURL(
          tmp->ptr, {}, {.no_referer = true}, nullptr)) {
    CurrentTab->pushBuffer(buf);
  }

  co_return;
}

// MOVE_LEFT
//"Cursor left"
std::shared_ptr<CoroutineState<void>> movL(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveCol(-m);
  co_return;
}

// MOVE_LEFT1
//"Cursor left. With edge touched, slide"
std::shared_ptr<CoroutineState<void>> movL1(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveCol(-m);
  co_return;
}

// MOVE_RIGHT
//"Cursor right"
std::shared_ptr<CoroutineState<void>> movR(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveCol(m);
  co_return;
}

// MOVE_RIGHT1
//"Cursor right. With edge touched, slide"
std::shared_ptr<CoroutineState<void>> movR1(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveCol(m);
  co_return;
}

// MOVE_DOWN
//"Cursor down"
std::shared_ptr<CoroutineState<void>> movD(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveRow(m);
  co_return;
}

// MOVE_DOWN1
//"Cursor down. With edge touched, slide"
std::shared_ptr<CoroutineState<void>> movD1(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveRow(m);
  co_return;
}

// MOVE_UP
//"Cursor up"
std::shared_ptr<CoroutineState<void>> movU(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveRow(-m);
  co_return;
}

// MOVE_UP1
//"Cursor up. With edge touched, slide"
std::shared_ptr<CoroutineState<void>> movU1(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveRow(-m);
  co_return;
}

// PREV_WORD
//"Move to the previous word"
std::shared_ptr<CoroutineState<void>> movLW(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int i, n = App::instance().searchKeyNum();

  if (buf->layout->empty())
    co_return;

  for (i = 0; i < n; i++) {
    auto pline = buf->layout->currentLine();
    auto ppos = buf->layout->cursorPos();

    if (buf->layout->prev_nonnull_line(buf->layout->currentLine()) < 0)
      co_return;

    while (1) {
      auto l = buf->layout->currentLine();
      auto lb = l->lineBuf();
      while (buf->layout->cursorPos() > 0) {
        int tmp = buf->layout->cursorPos();
        prevChar(tmp, l);
        if (is_wordchar(lb[tmp])) {
          break;
        }
        buf->layout->cursorPos(tmp);
      }
      if (buf->layout->cursorPos() > 0)
        break;
      auto prev = buf->layout->currentLine();
      --prev;
      if (buf->layout->prev_nonnull_line(prev) < 0) {
        buf->layout->visual.cursorRow(buf->layout->linenumber(pline));
        buf->layout->cursorPos(ppos);
        co_return;
      }
      buf->layout->cursorPos(buf->layout->currentLine()->len());
    }

    {
      auto l = buf->layout->currentLine();
      auto lb = l->lineBuf();
      while (buf->layout->cursorPos() > 0) {
        int tmp = buf->layout->cursorPos();
        prevChar(tmp, l);
        if (!is_wordchar(lb[tmp])) {
          break;
        }
        buf->layout->cursorPos(tmp);
      }
    }
  }
}

// NEXT_WORD
//"Move to the next word"
std::shared_ptr<CoroutineState<void>> movRW(const FuncContext &context) {
  //   Line *pline;
  //   int ppos;
  //   int i, n = App::instance().searchKeyNum();
  //
  //   if (CurrentTab->currentBuffer()->layout->empty())
  //     co_return;
  //
  //   for (i = 0; i < n; i++) {
  //     pline = CurrentTab->currentBuffer()->layout->currentLine();
  //     ppos = CurrentTab->currentBuffer()->layout->cursorPos();
  //
  //     if (CurrentTab->currentBuffer()->layout->next_nonnull_line(
  //             CurrentTab->currentBuffer()->layout->currentLine()) < 0)
  //       goto end;
  //
  //     auto l = CurrentTab->currentBuffer()->layout->currentLine();
  //     auto lb = l->lineBuf();
  //     while (CurrentTab->currentBuffer()->layout->cursorPos() < l->len() &&
  //            is_wordchar(lb[CurrentTab->currentBuffer()->layout->cursorPos()]))
  //            {
  //       nextChar(CurrentTab->currentBuffer()->layout->cursorPos(), l);
  //     }
  //
  //     while (1) {
  //       while (
  //           CurrentTab->currentBuffer()->layout->cursorPos() < l->len() &&
  //           !is_wordchar(lb[CurrentTab->currentBuffer()->layout->cursorPos()]))
  //           {
  //         nextChar(CurrentTab->currentBuffer()->layout->cursorPos(), l);
  //       }
  //       if (CurrentTab->currentBuffer()->layout->cursorPos() < l->len())
  //         break;
  //       auto next = CurrentTab->currentBuffer()->layout->currentLine();
  //       ++next;
  //       if (CurrentTab->currentBuffer()->layout->next_nonnull_line(next) < 0)
  //       {
  //         CurrentTab->currentBuffer()->layout->cursor.row =
  //             CurrentTab->currentBuffer()->layout->linenumber(pline) -
  //             CurrentTab->currentBuffer()->layout->_topLine;
  //         CurrentTab->currentBuffer()->layout->cursorPos() = ppos;
  //         goto end;
  //       }
  //       CurrentTab->currentBuffer()->layout->cursorPos() = 0;
  //       l = CurrentTab->currentBuffer()->layout->currentLine();
  //       lb = l->lineBuf();
  //     }
  //   }
  // end:
  //   CurrentTab->currentBuffer()->layout->arrangeCursor();
  co_return;
}

/* Quit */
// ABORT EXIT
//"Quit without confirmation"
std::shared_ptr<CoroutineState<void>> quitfm(const FuncContext &context) {
  App::instance().exit(0);
  co_return;
}

/* Question and Quit */
// QUIT
//"Quit with confirmation request"
std::shared_ptr<CoroutineState<void>> qquitfm(const FuncContext &context) {
  App::instance().exit(0);
  co_return;
}

/* Select buffer */
// SELECT
//"Display buffer-stack panel"
std::shared_ptr<CoroutineState<void>> selBuf(const FuncContext &context) {

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
std::shared_ptr<CoroutineState<void>> susp(const FuncContext &context) {
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
std::shared_ptr<CoroutineState<void>> goLine(const FuncContext &context) {

  auto str = App::instance().searchKeyData();
  if (prec_num)
    CurrentTab->currentBuffer()->layout->_goLine("^", prec_num);
  else if (str)
    CurrentTab->currentBuffer()->layout->_goLine(str, prec_num);
  else {
    // _goLine(inputStr("Goto line: ", ""));
  }
  co_return;
}

// BEGIN
//"Go to the first line"
std::shared_ptr<CoroutineState<void>> goLineF(const FuncContext &context) {
  CurrentTab->currentBuffer()->layout->_goLine("^", prec_num);
  co_return;
}

// END
//"Go to the last line"
std::shared_ptr<CoroutineState<void>> goLineL(const FuncContext &context) {
  CurrentTab->currentBuffer()->layout->_goLine("$", prec_num);
  co_return;
}

/* Go to the beginning of the line */
// LINE_BEGIN
//"Go to the beginning of the line"
std::shared_ptr<CoroutineState<void>> linbeg(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;
  CurrentTab->currentBuffer()->layout->cursorPos(0);
}

/* Go to the bottom of the line */
// LINE_END
//"Go to the end of the line"
std::shared_ptr<CoroutineState<void>> linend(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;
  CurrentTab->currentBuffer()->layout->cursorPos(
      CurrentTab->currentBuffer()->layout->currentLine()->len() - 1);
}

/* Run editor on the current buffer */
// EDIT
//"Edit local source"
std::shared_ptr<CoroutineState<void>> editBf(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  auto fn = buf->res->filename;

  if (fn.empty() || /* Behaving as a pager */
      (buf->res->type.empty() &&
       buf->res->edit == nullptr) || /* Reading shell */
      buf->res->currentURL.scheme != SCM_LOCAL ||
      buf->res->currentURL.file == "-" /* file is std input  */
  ) {
    App::instance().disp_err_message("Can't edit other than local file");
    co_return;
  }

  std::string cmd;
  if (buf->res->edit) {
    cmd = unquote_mailcap(buf->res->edit, buf->res->type.c_str(), fn.c_str(),
                          buf->res->getHeader("Content-Type:"), nullptr)
              ->ptr;
  } else {
    cmd = ioutil::myEditor(shell_quote(fn.c_str()),
                           buf->layout->visual.cursor().row);
  }
  exec_cmd(cmd);

  reload(context);
}

/* Run editor on the current screen */
// EDIT_SCREEN
//"Edit rendered copy of document"
std::shared_ptr<CoroutineState<void>> editScr(const FuncContext &context) {
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == nullptr) {
    App::instance().disp_err_message(
        Sprintf("Can't open %s", tmpf.c_str())->ptr);
    co_return;
  }
  CurrentTab->currentBuffer()->saveBuffer(f, true);
  fclose(f);
  exec_cmd(ioutil::myEditor(
      shell_quote(tmpf.c_str()),
      CurrentTab->currentBuffer()->layout->visual.cursor().row));
  unlink(tmpf.c_str());
}

/* follow HREF link */
// GOTO_LINK
//"Follow current hyperlink in a new buffer"
std::shared_ptr<CoroutineState<void>> followA(const FuncContext &context) {
  auto [a, buf] = co_await CurrentTab->currentBuffer()->followAnchor();
  if (buf) {
    App::instance().pushBuffer(buf, a->target);
  }

  co_return;
}

/* view inline image */
// VIEW_IMAGE
//"Display image in viewer"
std::shared_ptr<CoroutineState<void>> followI(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;

  auto a = CurrentTab->currentBuffer()->layout->retrieveCurrentImg();
  if (a == nullptr)
    co_return;
  App::instance().message(Sprintf("loading %s", a->url.c_str())->ptr);

  auto res =
      loadGeneralFile(a->url, CurrentTab->currentBuffer()->res->currentURL, {});
  if (!res) {
    char *emsg = Sprintf("Can't load %s", a->url.c_str())->ptr;
    App::instance().disp_err_message(emsg);
    co_return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { CurrentTab->pushBuffer(buf); }
}

/* submit form */
// SUBMIT
//"Submit form"
std::shared_ptr<CoroutineState<void>> submitForm(const FuncContext &context) {
  if (auto f = CurrentTab->currentBuffer()->layout->retrieveCurrentForm()) {
    auto buf = CurrentTab->currentBuffer()
                   ->followForm(f, true)
                   ->return_value()
                   .value();
    ;
    if (buf) {
      App::instance().pushBuffer(buf, f->target);
    }
  }
  co_return;
}

/* go to the top anchor */
// LINK_BEGIN
//"Move to the first hyperlink"
std::shared_ptr<CoroutineState<void>> topA(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  if (buf->layout->empty())
    co_return;
  if (hl->size() == 0)
    co_return;

  int hseq = 0;
  if (prec_num > (int)hl->size())
    hseq = hl->size() - 1;
  else if (prec_num > 0)
    hseq = prec_num - 1;

  Anchor *an = nullptr;
  BufferPoint *po = nullptr;
  do {
    if (hseq >= (int)hl->size())
      co_return;
    po = hl->get(hseq);
    if (buf->layout->data._href) {
      an = buf->layout->data._href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && buf->layout->data._formitem) {
      an = buf->layout->data._formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq++;
  } while (an == nullptr);

  buf->layout->visual.cursorRow(po->line);
  buf->layout->cursorPos(po->pos);
}

/* go to the last anchor */
// LINK_END
//"Move to the last hyperlink"
std::shared_ptr<CoroutineState<void>> lastA(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  if (buf->layout->empty())
    co_return;
  if (hl->size() == 0)
    co_return;

  int hseq;
  if (prec_num >= (int)hl->size())
    hseq = 0;
  else if (prec_num > 0)
    hseq = hl->size() - prec_num;
  else
    hseq = hl->size() - 1;

  Anchor *an = nullptr;
  ;
  BufferPoint *po = nullptr;
  do {
    if (hseq < 0)
      co_return;
    po = hl->get(hseq);
    if (buf->layout->data._href) {
      an = buf->layout->data._href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && buf->layout->data._formitem) {
      an = buf->layout->data._formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq--;
  } while (an == nullptr);

  buf->layout->visual.cursorRow(po->line);
  buf->layout->cursorPos(po->pos);
}

/* go to the nth anchor */
// LINK_N
//"Go to the nth link"
std::shared_ptr<CoroutineState<void>> nthA(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  int n = App::instance().searchKeyNum();
  if (n < 0 || n > (int)hl->size()) {
    co_return;
  }

  if (buf->layout->empty())
    co_return;
  if (!hl || hl->size() == 0)
    co_return;

  auto po = hl->get(n - 1);
  Anchor *an = nullptr;
  if (buf->layout->data._href) {
    an = buf->layout->data._href->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr && buf->layout->data._formitem) {
    an = buf->layout->data._formitem->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr)
    co_return;

  buf->layout->visual.cursorRow(po->line);
  buf->layout->cursorPos(po->pos);
}

/* go to the next anchor */
// NEXT_LINK
//"Move to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextA(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int n = App::instance().searchKeyNum();
  buf->layout->_nextA(buf->layout->data.baseURL, n);
  co_return;
}

/* go to the previous anchor */
// PREV_LINK
//"Move to the previous hyperlink"
std::shared_ptr<CoroutineState<void>> prevA(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  int n = App::instance().searchKeyNum();
  buf->layout->_prevA(buf->layout->data.baseURL, n);
  co_return;
}

/* go to the next left anchor */
// NEXT_LEFT
//"Move left to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextL(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextX(-1, 0, n);
  co_return;
}

/* go to the next left-up anchor */
// NEXT_LEFT_UP
//"Move left or upward to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextLU(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextX(-1, -1, n);
  co_return;
}

/* go to the next right anchor */
// NEXT_RIGHT
//"Move right to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextR(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextX(1, 0, n);
  co_return;
}

/* go to the next right-down anchor */
// NEXT_RIGHT_DOWN
//"Move right or downward to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextRD(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextX(1, 1, n);
  co_return;
}

/* go to the next downward anchor */
// NEXT_DOWN
//"Move downward to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextD(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextY(1, n);
  co_return;
}

/* go to the next upward anchor */
// NEXT_UP
//"Move upward to the next hyperlink"
std::shared_ptr<CoroutineState<void>> nextU(const FuncContext &context) {
  int n = App::instance().searchKeyNum();
  CurrentTab->currentBuffer()->layout->nextY(-1, n);
  co_return;
}

/* go to the next bufferr */
// NEXT
//"Switch to the next buffer"
std::shared_ptr<CoroutineState<void>> nextBf(const FuncContext &context) {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf = CurrentTab->forwardBuffer(CurrentTab->currentBuffer());
    if (!buf) {
      if (i == 0)
        co_return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
}

/* go to the previous bufferr */
// PREV
//"Switch to the previous buffer"
std::shared_ptr<CoroutineState<void>> prevBf(const FuncContext &context) {
  for (int i = 0; i < PREC_NUM; i++) {
    auto buf = CurrentTab->currentBuffer()->backBuffer;
    if (!buf) {
      if (i == 0)
        co_return;
      break;
    }
    CurrentTab->currentBuffer(buf);
  }
}

/* delete current buffer and back to the previous buffer */
// BACK
//"Close current buffer and return to the one below in stack"
std::shared_ptr<CoroutineState<void>> backBf(const FuncContext &context) {
  // Buffer *buf = CurrentTab->currentBuffer()->linkBuffer[LB_N_FRAME];

  if (!CurrentTab->currentBuffer()->checkBackBuffer()) {
    if (close_tab_back && App::instance().nTab() >= 1) {
      App::instance().deleteTab(CurrentTab);
    } else
      App::instance().disp_message("Can't go back...");
    co_return;
  }

  CurrentTab->deleteBuffer(CurrentTab->currentBuffer());
}

// DELETE_PREVBUF
//"Delete previous buffer (mainly for local CGI-scripts)"
std::shared_ptr<CoroutineState<void>>
deletePrevBuf(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer()->backBuffer;
  if (buf) {
    CurrentTab->deleteBuffer(buf);
  }
  co_return;
}

// GOTO
//"Open specified document in a new buffer"
std::shared_ptr<CoroutineState<void>> goURL(const FuncContext &context) {
  auto url = App::instance().searchKeyData();
  if (auto buf =
          CurrentTab->currentBuffer()->goURL0(url, "Goto URL: ", false)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

// GOTO_HOME
//"Open home page in a new buffer"
std::shared_ptr<CoroutineState<void>> goHome(const FuncContext &context) {
  const char *url = nullptr;
  if ((url = getenv("HTTP_HOME")) != nullptr ||
      (url = getenv("WWW_HOME")) != nullptr) {
    auto cur_buf = CurrentTab->currentBuffer();
    SKIP_BLANKS(url);
    Url p_url(url_quote(url));
    URLHist->push(p_url.to_Str());
    if (auto buf =
            CurrentTab->currentBuffer()->cmd_loadURL(url, {}, {}, nullptr)) {
      CurrentTab->pushBuffer(buf);
      URLHist->push(buf->res->currentURL.to_Str());
    }
  }
  co_return;
}

// GOTO_RELATIVE
//"Go to relative address"
std::shared_ptr<CoroutineState<void>> gorURL(const FuncContext &context) {
  auto url = App::instance().searchKeyData();
  if (auto buf = CurrentTab->currentBuffer()->goURL0(
          url, "Goto relative URL: ", true)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* load bookmark */
// BOOKMARK VIEW_BOOKMARK
//"View bookmarks"
std::shared_ptr<CoroutineState<void>> ldBmark(const FuncContext &context) {
  if (auto buf = CurrentTab->currentBuffer()->cmd_loadURL(
          BookmarkFile.c_str(), {}, {.no_referer = true}, nullptr)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* Add current to bookmark */
// ADD_BOOKMARK
//"Add current page to bookmarks"
std::shared_ptr<CoroutineState<void>> adBmark(const FuncContext &context) {
  auto tmp = Sprintf(
      "mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
      (form_quote(localCookie()->ptr)).c_str(),
      (form_quote(BookmarkFile)).c_str(),
      (form_quote(CurrentTab->currentBuffer()->res->currentURL.to_Str()))
          .c_str(),
      (form_quote(CurrentTab->currentBuffer()->layout->data.title)).c_str());
  auto request = std::make_shared<Form>("", "post", "", "", "");
  request->body = tmp->ptr;
  request->length = tmp->length;
  if (auto buf = CurrentTab->currentBuffer()->cmd_loadURL(
          "file:///$LIB/" W3MBOOKMARK_CMDNAME, {}, {.no_referer = true},
          request)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* option setting */
// OPTIONS
//"Display options setting panel"
std::shared_ptr<CoroutineState<void>> ldOpt(const FuncContext &context) {
  auto html = Option::instance().load_option_panel();
  auto newbuf =
      loadHTMLString(App::instance().INIT_BUFFER_WIDTH(), {}, html.c_str());
  CurrentTab->pushBuffer(newbuf);
  co_return;
}

/* set an option */
// SET_OPTION
//"Set option"
std::shared_ptr<CoroutineState<void>> setOpt(const FuncContext &context) {
  const char *opt;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  opt = App::instance().searchKeyData();
  if (opt == nullptr || *opt == '\0' || strchr(opt, '=') == nullptr) {
    if (opt != nullptr && *opt != '\0') {
      auto v = Option::instance().get_param_option(opt);
      opt = Sprintf("%s=%s", opt, v.c_str())->ptr;
    }
    // opt = inputStrHist("Set option: ", opt, TextHist);
    if (opt == nullptr || *opt == '\0') {
      co_return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
}

/* error message list */
// MSGS
//"Display error messages"
std::shared_ptr<CoroutineState<void>> msgs(const FuncContext &context) {
  auto buf =
      App::instance().message_list_panel(App::instance().INIT_BUFFER_WIDTH());
  CurrentTab->pushBuffer(buf);
  co_return;
}

/* page info */
// INFO
//"Display information about the current document"
std::shared_ptr<CoroutineState<void>> pginfo(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer()->page_info_panel(
      App::instance().INIT_BUFFER_WIDTH());
  CurrentTab->pushBuffer(buf);
  co_return;
}

/* link,anchor,image list */
// LIST
//"Show all URLs referenced"
std::shared_ptr<CoroutineState<void>> linkLst(const FuncContext &context) {
  if (auto buf = link_list_panel(App::instance().INIT_BUFFER_WIDTH(),
                                 CurrentTab->currentBuffer())) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* cookie list */
// COOKIE
//"View cookie list"
std::shared_ptr<CoroutineState<void>> cooLst(const FuncContext &context) {
  if (auto buf = cookie_list_panel(App::instance().INIT_BUFFER_WIDTH())) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* History page */
// HISTORY
//"Show browsing history"
std::shared_ptr<CoroutineState<void>> ldHist(const FuncContext &context) {
  auto html = URLHist->toHtml();
  if (auto buf =
          loadHTMLString(App::instance().INIT_BUFFER_WIDTH(), {}, html)) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

/* download HREF link */
// SAVE_LINK
//"Save hyperlink target"
std::shared_ptr<CoroutineState<void>> svA(const FuncContext &context) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followA(context);
  // do_download = false;
  co_return;
}

/* download IMG link */
// SAVE_IMAGE
//"Save inline image"
std::shared_ptr<CoroutineState<void>> svI(const FuncContext &context) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followI(context);
  // do_download = false;
  co_return;
}

/* save buffer */
// PRINT SAVE_SCREEN
//"Save rendered document"
std::shared_ptr<CoroutineState<void>> svBuf(const FuncContext &context) {
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
  CurrentTab->currentBuffer()->saveBuffer(f, true);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
#endif

  co_return;
  ;
}

/* save source */
// DOWNLOAD SAVE
//"Save document source"
std::shared_ptr<CoroutineState<void>> svSrc(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->res->sourcefile.empty()) {
    co_return;
  }
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;

  const char *file;
  if (CurrentTab->currentBuffer()->res->currentURL.scheme == SCM_LOCAL) {
    file = guess_filename(
        CurrentTab->currentBuffer()->res->currentURL.real_file.c_str());
  } else {
    file = CurrentTab->currentBuffer()->res->guess_save_name(
        CurrentTab->currentBuffer()->res->currentURL.file.c_str());
  }
  doFileCopy(CurrentTab->currentBuffer()->res->sourcefile.c_str(), file);
  PermitSaveToPipe = false;
}

/* peek URL */
// PEEK_LINK
//"Show target address"
std::shared_ptr<CoroutineState<void>> peekURL(const FuncContext &context) {
  App::instance().peekURL();
  co_return;
}

// PEEK
//"Show current address"
std::shared_ptr<CoroutineState<void>> curURL(const FuncContext &context) {
  auto url = App::instance().currentUrl();
  App::instance().disp_message(url.c_str());
  co_return;
}

/* view HTML source */
// SOURCE VIEW
//"Toggle between HTML shown or processed"
std::shared_ptr<CoroutineState<void>> vwSrc(const FuncContext &context) {

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
std::shared_ptr<CoroutineState<void>> reload(const FuncContext &context) {

  if (CurrentTab->currentBuffer()->res->currentURL.scheme == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't reload stdin");
    co_return;
  }

  auto sbuf = Buffer::create(0);
  *sbuf = *CurrentTab->currentBuffer();

  bool multipart = false;
  std::shared_ptr<Form> request;
  if (CurrentTab->currentBuffer()->layout->data.form_submit) {
    request = CurrentTab->currentBuffer()->layout->data.form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      multipart = true;
      CurrentTab->currentBuffer()
          ->layout->data.form_submit->query_from_followform_multipart();
      struct stat st;
      stat(request->body, &st);
      request->length = st.st_size;
    }
  } else {
    request = nullptr;
  }

  auto url = Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str());
  App::instance().message("Reloading...");
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
    vwSrc(context);
    if (CurrentTab->currentBuffer() != buf) {
      CurrentTab->deleteBuffer(buf);
    }
  }
  CurrentTab->currentBuffer()->layout->data.form_submit =
      sbuf->layout->data.form_submit;
  if (CurrentTab->currentBuffer()->layout->firstLine()) {
    // CurrentTab->currentBuffer()->layout->COPY_BUFROOT_FROM(sbuf->layout);
    CurrentTab->currentBuffer()->layout->visual.restorePosition(
        sbuf->layout->visual);
  }
}

/* reshape */
// RESHAPE
//"Re-render document"
std::shared_ptr<CoroutineState<void>> reshape(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  buf->layout->data.need_reshape = true;
  if (buf->layout->data.need_reshape) {
    auto visual = buf->layout->visual;
    auto body = buf->res->getBody();
    if (buf->res->is_html_type()) {
      buf->layout = loadHTMLstream(App::instance().INIT_BUFFER_WIDTH(),
                                   buf->res->currentURL, body);
    } else {
      loadBuffer(buf->layout, buf->res.get(), body);
    }
    buf->layout->visual = visual;
  }
  co_return;
}

// MARK_URL
//"Turn URL-like strings into hyperlinks"
// std::shared_ptr<CoroutineState<void>> chkURL(const FuncContext &context) {
//   CurrentTab->currentBuffer()->layout->chkURLBuffer();
//   co_return;
// }

// MARK_WORD
//"Turn current word into hyperlink"
// std::shared_ptr<CoroutineState<void>> chkWORD(const FuncContext &context) {
//   int spos, epos;
//   auto p = CurrentTab->currentBuffer()->layout->getCurWord(&spos, &epos);
//   if (p.empty())
//     co_return;
//   CurrentTab->currentBuffer()->layout->data.reAnchorWord(
//       CurrentTab->currentBuffer()->layout->currentLine(), spos, epos);
// }

/* render frames */
// FRAME
//"Toggle rendering HTML frames"
std::shared_ptr<CoroutineState<void>> rFrame(const FuncContext &context) {
  co_return;
}

// EXTERN
//"Display using an external browser"
std::shared_ptr<CoroutineState<void>> extbrz(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->res->currentURL.scheme == SCM_LOCAL &&
      CurrentTab->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't browse stdin");
    co_return;
  }
  invoke_browser(CurrentTab->currentBuffer()->res->currentURL.to_Str().c_str());
}

// EXTERN_LINK
//"Display target using an external browser"
std::shared_ptr<CoroutineState<void>> linkbrz(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;

  auto a = buf->layout->retrieveCurrentAnchor();
  if (a == nullptr)
    co_return;

  Url pu(a->url, buf->layout->data.baseURL);
  invoke_browser(pu.to_Str().c_str());
}

/* show current line number and number of lines in the entire document */
// LINE_INFO
//"Display current position in document"
std::shared_ptr<CoroutineState<void>> curlno(const FuncContext &context) {
  auto layout = CurrentTab->currentBuffer()->layout;
  Line *l = layout->currentLine();
  Str *tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != nullptr) {
    cur = layout->linenumber(l);
    ;
    col = layout->visual.cursor().col + 1;
    len = l->width();
  }
  if (layout->lastLine())
    all = layout->linenumber(layout->lastLine());
  tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col,
                len);

  App::instance().disp_message(tmp->ptr);
  co_return;
}

// VERSION
//"Display the version of w3m"
std::shared_ptr<CoroutineState<void>> dispVer(const FuncContext &context) {
  App::instance().disp_message(Sprintf("w3m version %s", w3m_version)->ptr);
  co_return;
}

// WRAP_TOGGLE
//"Toggle wrapping mode in searches"
std::shared_ptr<CoroutineState<void>> wrapToggle(const FuncContext &context) {
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
std::shared_ptr<CoroutineState<void>> dictword(const FuncContext &context) {
  // auto buf = execdict(inputStr("(dictionary)!", ""));
  // CurrentTab->pushBuffer(buf);
  // App::instance().invalidate();

  co_return;
}

// DICT_WORD_AT
//"Execute dictionary command for word at cursor"
std::shared_ptr<CoroutineState<void>> dictwordat(const FuncContext &context) {
  auto word = CurrentTab->currentBuffer()->layout->getCurWord();
  if (auto buf = execdict(word.c_str())) {
    CurrentTab->pushBuffer(buf);
  }
  co_return;
}

// COMMAND
//"Invoke w3m function(s)"
std::shared_ptr<CoroutineState<void>> execCmd(const FuncContext &context) {
  App::instance().doCmd();
  co_return;
}

// ALARM
//"Set alarm"
std::shared_ptr<CoroutineState<void>> setAlarm(const FuncContext &context) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == nullptr) {
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
std::shared_ptr<CoroutineState<void>> reinit(const FuncContext &context) {
  auto resource = App::instance().searchKeyData();

  if (resource == nullptr) {
    init_rc();
    sync_with_option();
    initCookie();
    co_return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
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
std::shared_ptr<CoroutineState<void>> defKey(const FuncContext &context) {
  const char *data;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  data = App::instance().searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data == nullptr || *data == '\0') {
      co_return;
    }
  }
  App::instance().setKeymap(allocStr(data, -1), -1, true);
}

// NEW_TAB
//"Open a new tab (with current document)"
std::shared_ptr<CoroutineState<void>> newT(const FuncContext &context) {
  App::instance().newTab();
  co_return;
}

// CLOSE_TAB
//"Close tab"
std::shared_ptr<CoroutineState<void>> closeT(const FuncContext &context) {

  if (App::instance().nTab() <= 1)
    co_return;

  std::shared_ptr<TabBuffer> tab;
  if (prec_num)
    tab = App::instance().numTab(PREC_NUM);
  else
    tab = CurrentTab;
  if (tab)
    App::instance().deleteTab(tab);
}

// NEXT_TAB
//"Switch to the next tab"
std::shared_ptr<CoroutineState<void>> nextT(const FuncContext &context) {
  App::instance().nextTab(PREC_NUM);
  co_return;
}

// PREV_TAB
//"Switch to the previous tab"
std::shared_ptr<CoroutineState<void>> prevT(const FuncContext &context) {
  App::instance().prevTab(PREC_NUM);
  co_return;
}

// TAB_LINK
//"Follow current hyperlink in a new tab"
std::shared_ptr<CoroutineState<void>> tabA(const FuncContext &context) {
  auto [a, buf] = co_await CurrentTab->currentBuffer()->followAnchor(false);
  if (buf) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_GOTO
//"Open specified document in a new tab"
std::shared_ptr<CoroutineState<void>> tabURL(const FuncContext &context) {
  auto url = App::instance().searchKeyData();
  if (auto buf = CurrentTab->currentBuffer()->goURL0(
          url, "Goto URL on new tab: ", false)) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_GOTO_RELATIVE
//"Open relative address in a new tab"
std::shared_ptr<CoroutineState<void>> tabrURL(const FuncContext &context) {
  auto url = App::instance().searchKeyData();
  if (auto buf = CurrentTab->currentBuffer()->goURL0(
          url, "Goto relative URL on new tab: ", true)) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_RIGHT
//"Move right along the tab bar"
std::shared_ptr<CoroutineState<void>> tabR(const FuncContext &context) {
  App::instance().tabRight(PREC_NUM);
  co_return;
}

// TAB_LEFT
//"Move left along the tab bar"
std::shared_ptr<CoroutineState<void>> tabL(const FuncContext &context) {
  App::instance().tabLeft(PREC_NUM);
  co_return;
}

/* download panel */
// DOWNLOAD_LIST
//"Display downloads panel"
std::shared_ptr<CoroutineState<void>> ldDL(const FuncContext &context) {
  int replace = false, new_tab = false;

  if (!FirstDL) {
    if (replace) {
      if (CurrentTab->currentBuffer() == CurrentTab->firstBuffer &&
          CurrentTab->currentBuffer()->backBuffer == nullptr) {
        if (App::instance().nTab() > 1)
          App::instance().deleteTab(CurrentTab);
      } else
        CurrentTab->deleteBuffer(CurrentTab->currentBuffer());
    }
    co_return;
  }

  auto reload = checkDownloadList();
  auto html = DownloadListBuffer();
  if (html.empty()) {
    co_return;
  }
  auto buf = Buffer::create();
  buf->layout = loadHTMLstream(App::instance().INIT_BUFFER_WIDTH(),
                               buf->res->currentURL, html, true);
  if (replace) {
    // buf->layout->COPY_BUFROOT_FROM(CurrentTab->currentBuffer()->layout);
    buf->layout->visual.restorePosition(
        CurrentTab->currentBuffer()->layout->visual);
  }
  if (!replace && open_tab_dl_list) {
    App::instance().newTab();
    new_tab = true;
  }
  CurrentTab->pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf(context);
  if (reload) {
    // CurrentTab->currentBuffer()->layout->event =
    //     setAlarmEvent(CurrentTab->currentBuffer()->layout->event, 1,
    //     AL_IMPLICIT,
    //                   FUNCNAME_reload, nullptr);
  }
}

// UNDO
//"Cancel the last cursor movement"
std::shared_ptr<CoroutineState<void>> undoPos(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;

  // CurrentTab->currentBuffer()->layout->undoPos(PREC_NUM);
}

// REDO
//"Cancel the last undo"
std::shared_ptr<CoroutineState<void>> redoPos(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;

  // CurrentTab->currentBuffer()->layout->redoPos(PREC_NUM);
}

// CURSOR_TOP
//"Move cursor to the top of the screen"
std::shared_ptr<CoroutineState<void>> cursorTop(const FuncContext &context) {
  if (CurrentTab->currentBuffer()->layout->empty())
    co_return;

  CurrentTab->currentBuffer()->layout->visual.cursorRow(0);
}

// CURSOR_MIDDLE
//"Move cursor to the middle of the screen"
std::shared_ptr<CoroutineState<void>> cursorMiddle(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = (buf->layout->LINES() - 1) / 2;
  CurrentTab->currentBuffer()->layout->visual.cursorMoveRow(offsety);
}

// CURSOR_BOTTOM
//"Move cursor to the bottom of the screen"
std::shared_ptr<CoroutineState<void>> cursorBottom(const FuncContext &context) {
  auto buf = CurrentTab->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = buf->layout->LINES() - 1;
  CurrentTab->currentBuffer()->layout->visual.cursorMoveRow(offsety);
}

/* follow HREF link in the buffer */
std::shared_ptr<CoroutineState<void>> bufferA(const FuncContext &context) {
  on_target = false;
  followA(context);
  on_target = true;
  co_return;
}
