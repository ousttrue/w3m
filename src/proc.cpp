#include "proc.h"
#include "tabbuffer.h"
#include "buffer.h"

static std::string s_searchString;
// if (str != nullptr && str != SearchString)
//   SearchString = str;
// if (SearchString == nullptr || *SearchString == '\0')
//   return SR_NOTFOUND;
//
// str = SearchString;

// NOTHING nullptr @ @ @
//"Do nothing"
std::shared_ptr<CoroutineState<void>>
nulcmd(const std::shared_ptr<IPlatform> &platform) { /* do nothing */
  co_return;
}

/* Move page forward */
// NEXT_PAGE
//"Scroll down one page"
std::shared_ptr<CoroutineState<void>>
pgFore(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.scrollMoveRow(buf->layout->LINES());
  co_return;
}

/* Move page backward */
// PREV_PAGE
//"Scroll up one page"
std::shared_ptr<CoroutineState<void>>
pgBack(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.scrollMoveRow(-buf->layout->LINES());
  co_return;
}

/* Move half page forward */
// NEXT_HALF_PAGE
//"Scroll down half a page"
std::shared_ptr<CoroutineState<void>>
hpgFore(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.scrollMoveRow((buf->layout->LINES() / 2));
  co_return;
}

/* Move half page backward */
// PREV_HALF_PAGE
//"Scroll up half a page"
std::shared_ptr<CoroutineState<void>>
hpgBack(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.scrollMoveRow(-buf->layout->LINES() / 2);
  co_return;
}

/* 1 line up */
// UP
//"Scroll the screen up one line"
std::shared_ptr<CoroutineState<void>>
lup1(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->visual.scrollMoveRow(1);
  co_return;
}

/* 1 line down */
// DOWN
//"Scroll the screen down one line"
std::shared_ptr<CoroutineState<void>>
ldown1(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->visual.scrollMoveRow(-1);
  co_return;
}

/* move cursor position to the center of screen */
// CENTER_V
//"Center on cursor line"
std::shared_ptr<CoroutineState<void>>
ctrCsrV(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = buf->layout->LINES() / 2 - buf->layout->visual.cursor().row;
  if (offsety != 0) {
    buf->layout->visual.scrollMoveRow(-offsety);
  }
}

// CENTER_H
//"Center on cursor column"
std::shared_ptr<CoroutineState<void>>
ctrCsrH(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
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
std::shared_ptr<CoroutineState<void>>
rdrwSc(const std::shared_ptr<IPlatform> &platform) {
  // clear();
  co_return;
}

#include "quote.h"
#include "option.h"
#include "ioutil.h"
#include "dict.h"
#include "app.h"
#include "file_util.h"
#include "html_feed_env.h"
#include "http_response.h"
#include "url_quote.h"
#include "search.h"
#include "mimetypes.h"
#include "linein.h"
#include "cookie.h"
#include "rc.h"
#include "form.h"
#include "history.h"
#include "anchorlist.h"
#include "etc.h"
#include "mailcap.h"
#include "cmp.h"
#include "http_request.h"
#include "http_response.h"
#include "http_session.h"
#include "local_cgi.h"
#include "search.h"
#include "myctype.h"
// #include <sys/stat.h>

// SEARCH SEARCH_FORE WHEREIS
//"Search forward"
std::shared_ptr<CoroutineState<void>>
srchfor(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();

  // bool disp = false;
  auto str = App::instance().searchKeyData();
  if (str.empty()) {
    // str = inputStrHist(prompt, nullptr, TextHist);
  }
  if (str.empty()) {
    str = s_searchString;
  }
  if (str.empty()) {
    co_return;
  }
  // disp = true;

  srch(buf->layout, forwardSearch, "Forward: ");
  co_return;
}

// ISEARCH
//"Incremental search forward"
std::shared_ptr<CoroutineState<void>>
isrchfor(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  isrch(buf->layout, forwardSearch, "I-search: ");
  co_return;
}

/* Search regular expression backward */

// SEARCH_BACK
//"Search backward"
std::shared_ptr<CoroutineState<void>>
srchbak(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  srch(buf->layout, backwardSearch, "Backward: ");
  co_return;
}

// ISEARCH_BACK
//"Incremental search backward"
std::shared_ptr<CoroutineState<void>>
isrchbak(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  isrch(buf->layout, backwardSearch, "I-search backward: ");
  co_return;
}

/* Search next matching */
// SEARCH_NEXT
//"Continue search forward"
std::shared_ptr<CoroutineState<void>>
srchnxt(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  srch_nxtprv(buf->layout, s_searchString, 0);
  co_return;
}

/* Search previous matching */
// SEARCH_PREV
//"Continue search backward"
std::shared_ptr<CoroutineState<void>>
srchprv(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  srch_nxtprv(buf->layout, s_searchString, 1);
  co_return;
}

/* Shift screen left */
// SHIFT_LEFT
//"Shift screen left"
std::shared_ptr<CoroutineState<void>>
shiftl(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;
  int column = buf->layout->visual.cursor().col;
  buf->layout->visual.cursorMoveCol(-buf->layout->COLS());
  buf->layout->visual.cursorMoveCol(-column);
}

/* Shift screen right */
// SHIFT_RIGHT
//"Shift screen right"
std::shared_ptr<CoroutineState<void>>
shiftr(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;
  // int column =
  // platform->currentTab()->currentBuffer()->layout->currentColumn;
  // platform->currentTab()->currentBuffer()->layout->columnSkip(
  //     App::instance().searchKeyNum() *
  //         (App::instance().contentCols() - 1) -
  //     1);
  // platform->currentTab()->currentBuffer()->layout->shiftvisualpos(
  //     platform->currentTab()->currentBuffer()->layout->currentColumn -
  //     column);
}

// RIGHT
//"Shift screen one column right"
std::shared_ptr<CoroutineState<void>>
col1R(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  Line *l = buf->layout->currentLine();
  int j, column;
  // int n = App::instance().searchKeyNum();
  //
  // if (l == nullptr)
  //   co_return;
  // for (j = 0; j < n; j++) {
  //   // column = buf->layout->currentColumn;
  //   // platform->currentTab()->currentBuffer()->layout->columnSkip(1);
  //   // if (column == buf->layout->currentColumn)
  //   //   break;
  //   // platform->currentTab()->currentBuffer()->layout->shiftvisualpos(1);
  // }
  co_return;
}

// LEFT
//"Shift screen one column left"
std::shared_ptr<CoroutineState<void>>
col1L(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  Line *l = buf->layout->currentLine();
  // int j, n = App::instance().searchKeyNum();
  //
  // if (l == nullptr)
  //   co_return;
  // for (j = 0; j < n; j++) {
  //   // if (buf->layout->currentColumn == 0)
  //   //   break;
  //   // platform->currentTab()->currentBuffer()->layout->columnSkip(-1);
  //   // platform->currentTab()->currentBuffer()->layout->shiftvisualpos(-1);
  // }
  co_return;
}

// SETENV
//"Set environment variable"
std::shared_ptr<CoroutineState<void>>
setEnv(const std::shared_ptr<IPlatform> &platform) {

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto env = App::instance().searchKeyData();
  if (env.empty() || strchr(env.c_str(), '=') == nullptr) {
    if (env.size())
      env += "=";
    // env = inputStrHist("Set environ: ", env, TextHist);
    if (env.empty()) {
      co_return;
    }
  }

  const char *value;
  if ((value = strchr(env.c_str(), '=')) != nullptr && value > env) {
    auto var = env.substr(0, value - env.c_str());
    value++;
    set_environ(var.c_str(), value);
  }
}

// PIPE_BUF
//"Pipe current buffer through a shell command and display output"
std::shared_ptr<CoroutineState<void>>
pipeBuf(const std::shared_ptr<IPlatform> &platform) {
  co_return;
}

/* Execute shell command and read output ac pipe. */
// PIPE_SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>>
pipesh(const std::shared_ptr<IPlatform> &platform) {
  co_return;
}

/* Execute shell command and load entire output to buffer */
// READ_SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>>
readsh(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = App::instance().searchKeyData();
  if (cmd.empty()) {
    // cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd.empty()) {
    co_return;
  }
  // MySignalHandler prevtrap = {};
  // prevtrap = mySignal(SIGINT, intTrap);
  // crmode();
  auto res = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  // term_raw();
  if (!res) {
    App::instance().disp_message("Execution failed");
    co_return;
  }

  auto buf = Buffer::create(res);
  platform->currentTab()->pushBuffer(buf);
}

/* Execute shell command */
// EXEC_SHELL SHELL
//"Execute shell command and display output"
std::shared_ptr<CoroutineState<void>>
execsh(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto cmd = App::instance().searchKeyData();
  if (cmd.empty()) {
    // cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd.empty()) {
    // App::instance().endRawMode();
    printf("\n");
    system(cmd.c_str()); /* We do not care about the exit code here! */
    printf("\n[Hit any key]");
    fflush(stdout);
    // App::instance().beginRawMode();
    // getch();
  }
  co_return;
}

/* Load file */
// LOAD
//"Open local file in a new buffer"
std::shared_ptr<CoroutineState<void>>
ldfile(const std::shared_ptr<IPlatform> &platform) {
  auto fn = App::instance().searchKeyData();
  // if (fn == nullptr || *fn == '\0') {
  //   fn = inputFilenameHist("(Load)Filename? ", nullptr, LoadHist);
  // }
  if (fn.empty()) {
    co_return;
  }
  if (auto res = loadGeneralFile(file_to_url(fn), {}, {.no_referer = true})) {
    auto buf = Buffer::create(res);
    platform->currentTab()->pushBuffer(buf);
  } else {
    // char *emsg = Sprintf("%s not found", fn.c_str())->ptr;
    // App::instance().disp_err_message(emsg);
  }
}

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

/* Load help file */
// HELP
//"Show help panel"
std::shared_ptr<CoroutineState<void>>
ldhelp(const std::shared_ptr<IPlatform> &platform) {
  auto lang = AcceptLang;
  auto n = strcspn(lang.c_str(), ";, \t");
  std::stringstream tmp;
  tmp << "file:///$LIB/" HELP_CGI CGI_EXTENSION "?version="
      << form_quote(w3m_version)
      << "&lang=" << form_quote(std::string_view(lang.c_str(), n));
  if (auto buf = platform->currentTab()->currentBuffer()->cmd_loadURL(
          tmp.str().c_str(), {}, {.no_referer = true}, nullptr)) {
    platform->currentTab()->pushBuffer(buf);
  }

  co_return;
}

// MOVE_LEFT
//"Cursor left"
std::shared_ptr<CoroutineState<void>>
movL(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  // int m = App::instance().searchKeyNum();
  buf->layout->visual.cursorMoveCol(-1);
  co_return;
}

// MOVE_LEFT1
//"Cursor left. With edge touched, slide"
std::shared_ptr<CoroutineState<void>>
movL1(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveCol(-1);
  co_return;
}

// MOVE_RIGHT
//"Cursor right"
std::shared_ptr<CoroutineState<void>>
movR(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveCol(1);
  co_return;
}

// MOVE_RIGHT1
//"Cursor right. With edge touched, slide"
std::shared_ptr<CoroutineState<void>>
movR1(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveCol(1);
  co_return;
}

// MOVE_DOWN
//"Cursor down"
std::shared_ptr<CoroutineState<void>>
movD(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveRow(1);
  co_return;
}

// MOVE_DOWN1
//"Cursor down. With edge touched, slide"
std::shared_ptr<CoroutineState<void>>
movD1(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveRow(1);
  co_return;
}

// MOVE_UP
//"Cursor up"
std::shared_ptr<CoroutineState<void>>
movU(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveRow(-1);
  co_return;
}

// MOVE_UP1
//"Cursor up. With edge touched, slide"
std::shared_ptr<CoroutineState<void>>
movU1(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->visual.cursorMoveRow(-1);
  co_return;
}

// PREV_WORD
//"Move to the previous word"
std::shared_ptr<CoroutineState<void>>
movLW(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();

  if (buf->layout->empty())
    co_return;

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

// NEXT_WORD
//"Move to the next word"
std::shared_ptr<CoroutineState<void>>
movRW(const std::shared_ptr<IPlatform> &platform) {
  //   Line *pline;
  //   int ppos;
  //   int i, n = App::instance().searchKeyNum();
  //
  //   if (platform->currentTab()->currentBuffer()->layout->empty())
  //     co_return;
  //
  //   for (i = 0; i < n; i++) {
  //     pline = platform->currentTab()->currentBuffer()->layout->currentLine();
  //     ppos = platform->currentTab()->currentBuffer()->layout->cursorPos();
  //
  //     if (platform->currentTab()->currentBuffer()->layout->next_nonnull_line(
  //             platform->currentTab()->currentBuffer()->layout->currentLine())
  //             < 0)
  //       goto end;
  //
  //     auto l =
  //     platform->currentTab()->currentBuffer()->layout->currentLine(); auto lb
  //     = l->lineBuf(); while
  //     (platform->currentTab()->currentBuffer()->layout->cursorPos() <
  //     l->len() &&
  //            is_wordchar(lb[platform->currentTab()->currentBuffer()->layout->cursorPos()]))
  //            {
  //       nextChar(platform->currentTab()->currentBuffer()->layout->cursorPos(),
  //       l);
  //     }
  //
  //     while (1) {
  //       while (
  //           platform->currentTab()->currentBuffer()->layout->cursorPos() <
  //           l->len() &&
  //           !is_wordchar(lb[platform->currentTab()->currentBuffer()->layout->cursorPos()]))
  //           {
  //         nextChar(platform->currentTab()->currentBuffer()->layout->cursorPos(),
  //         l);
  //       }
  //       if (platform->currentTab()->currentBuffer()->layout->cursorPos() <
  //       l->len())
  //         break;
  //       auto next =
  //       platform->currentTab()->currentBuffer()->layout->currentLine();
  //       ++next;
  //       if
  //       (platform->currentTab()->currentBuffer()->layout->next_nonnull_line(next)
  //       < 0)
  //       {
  //         platform->currentTab()->currentBuffer()->layout->cursor.row =
  //             platform->currentTab()->currentBuffer()->layout->linenumber(pline)
  //             - platform->currentTab()->currentBuffer()->layout->_topLine;
  //         platform->currentTab()->currentBuffer()->layout->cursorPos() =
  //         ppos; goto end;
  //       }
  //       platform->currentTab()->currentBuffer()->layout->cursorPos() = 0;
  //       l = platform->currentTab()->currentBuffer()->layout->currentLine();
  //       lb = l->lineBuf();
  //     }
  //   }
  // end:
  //   platform->currentTab()->currentBuffer()->layout->arrangeCursor();
  co_return;
}

// void App::exit(int) {
// const char *ans = "y";
// if (checkDownloadList()) {
//   ans = inputChar("Download process retains. "
//                   "Do you want to exit w3m? (y/n)");
// } else if (confirm) {
//   ans = inputChar("Do you want to exit w3m? (y/n)");
// }

// if (!(ans && TOLOWER(*ans) == 'y')) {
//   // cancel
//   App::instance().invalidate();
//   return;
// }

// App::instance().exit(0);
// stop libuv
// exit(i);
// uv_read_stop((uv_stream_t *)&g_tty_in);
// uv_signal_stop(&g_signal_resize);
// uv_timer_stop(&g_timer);
// }

/* Quit */
// ABORT EXIT
//"Quit without confirmation"
std::shared_ptr<CoroutineState<void>>
quitfm(const std::shared_ptr<IPlatform> &platform) {
  platform->quit();
  co_return;
}

/* Question and Quit */
// QUIT
//"Quit with confirmation request"
std::shared_ptr<CoroutineState<void>>
qquitfm(const std::shared_ptr<IPlatform> &platform) {
  platform->quit();
  co_return;
}

/* Select buffer */
// SELECT
//"Display buffer-stack panel"
std::shared_ptr<CoroutineState<void>>
selBuf(const std::shared_ptr<IPlatform> &platform) {

  // auto ok = false;
  // do {
  //   char cmd;
  //   auto buf = selectBuffer(platform->currentTab()->firstBuffer,
  //                           platform->currentTab()->currentBuffer(), &cmd);
  //   ok = platform->currentTab()->select(cmd, buf);
  // } while (!ok);
  //
  // App::instance().invalidate();
  co_return;
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
// INTERRUPT SUSPEND
//"Suspend w3m to background"
std::shared_ptr<CoroutineState<void>>
susp(const std::shared_ptr<IPlatform> &platform) {
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
std::shared_ptr<CoroutineState<void>>
goLine(const std::shared_ptr<IPlatform> &platform) {
  auto str = App::instance().searchKeyData();
  if (str.empty()) {
    // _goLine(inputStr("Goto line: ", ""));
  }
  platform->currentTab()->currentBuffer()->layout->_goLine(str);
  co_return;
}

// BEGIN
//"Go to the first line"
std::shared_ptr<CoroutineState<void>>
goLineF(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->_goLine("^");
  co_return;
}

// END
//"Go to the last line"
std::shared_ptr<CoroutineState<void>>
goLineL(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->_goLine("$");
  co_return;
}

/* Go to the beginning of the line */
// LINE_BEGIN
//"Go to the beginning of the line"
std::shared_ptr<CoroutineState<void>>
linbeg(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;
  platform->currentTab()->currentBuffer()->layout->cursorPos(0);
}

/* Go to the bottom of the line */
// LINE_END
//"Go to the end of the line"
std::shared_ptr<CoroutineState<void>>
linend(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;
  platform->currentTab()->currentBuffer()->layout->cursorPos(
      platform->currentTab()->currentBuffer()->layout->currentLine()->len() -
      1);
}

/* Run editor on the current buffer */
// EDIT
//"Edit local source"
std::shared_ptr<CoroutineState<void>>
editBf(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
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
    cmd =
        unquote_mailcap(buf->res->edit, buf->res->type.c_str(), fn.c_str(),
                        buf->res->getHeader("Content-Type:").c_str(), nullptr);
  } else {
    cmd = ioutil::myEditor(shell_quote(fn.c_str()),
                           buf->layout->visual.cursor().row);
  }

  platform->exec(cmd);

  reload(platform);
}

/* Run editor on the current screen */
// EDIT_SCREEN
//"Edit rendered copy of document"
std::shared_ptr<CoroutineState<void>>
editScr(const std::shared_ptr<IPlatform> &platform) {
  // auto tmpf = TmpFile::instance().tmpfname(TMPF_DFL, {});
  // auto f = fopen(tmpf.c_str(), "w");
  // if (f == nullptr) {
  //   // App::instance().disp_err_message(
  //   //     Sprintf("Can't open %s", tmpf.c_str())->ptr);
  //   co_return;
  // }
  // platform->platform->currentTab()()->currentBuffer()->saveBuffer(f, true);
  // fclose(f);
  // platform->exec(ioutil::myEditor(
  //     shell_quote(tmpf.c_str()),
  //     platform->currentTab()->currentBuffer()->layout->visual.cursor().row));
  // unlink(tmpf.c_str());
  co_return;
}

/* follow HREF link */
// GOTO_LINK
//"Follow current hyperlink in a new buffer"
std::shared_ptr<CoroutineState<void>>
followA(const std::shared_ptr<IPlatform> &platform) {
  auto [a, buf] =
      co_await platform->currentTab()->currentBuffer()->followAnchor();
  if (buf) {
    App::instance().pushBuffer(buf, a->target);
  }

  co_return;
}

/* view inline image */
// VIEW_IMAGE
//"Display image in viewer"
std::shared_ptr<CoroutineState<void>>
followI(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;

  auto a =
      platform->currentTab()->currentBuffer()->layout->retrieveCurrentImg();
  if (a == nullptr)
    co_return;
  // App::instance().message(Sprintf("loading %s", a->url.c_str())->ptr);

  auto res = loadGeneralFile(
      a->url, platform->currentTab()->currentBuffer()->res->currentURL, {});
  if (!res) {
    // char *emsg = Sprintf("Can't load %s", a->url.c_str())->ptr;
    // App::instance().disp_err_message(emsg);
    co_return;
  }

  auto buf = Buffer::create(res);
  // if (buf != NO_BUFFER)
  { platform->currentTab()->pushBuffer(buf); }
}

/* submit form */
// SUBMIT
//"Submit form"
std::shared_ptr<CoroutineState<void>>
submitForm(const std::shared_ptr<IPlatform> &platform) {
  if (auto f = platform->currentTab()
                   ->currentBuffer()
                   ->layout->retrieveCurrentForm()) {
    auto buf = platform->currentTab()
                   ->currentBuffer()
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
std::shared_ptr<CoroutineState<void>>
topA(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  if (buf->layout->empty())
    co_return;
  if (hl->size() == 0)
    co_return;

  int hseq = 0;
  Anchor *an = nullptr;
  BufferPoint *po = nullptr;
  do {
    if (hseq >= (int)hl->size())
      co_return;
    po = hl->get(hseq);
    if (buf->layout->data._href) {
      an = buf->layout->data._href->retrieveAnchor(*po);
    }
    if (an == nullptr && buf->layout->data._formitem) {
      an = buf->layout->data._formitem->retrieveAnchor(*po);
    }
    hseq++;
  } while (an == nullptr);

  buf->layout->visual.cursorRow(po->line);
  buf->layout->cursorPos(po->pos);
}

/* go to the last anchor */
// LINK_END
//"Move to the last hyperlink"
std::shared_ptr<CoroutineState<void>>
lastA(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  if (buf->layout->empty())
    co_return;
  if (hl->size() == 0)
    co_return;

  int hseq = hl->size() - 1;
  Anchor *an = nullptr;
  ;
  BufferPoint *po = nullptr;
  do {
    if (hseq < 0)
      co_return;
    po = hl->get(hseq);
    if (buf->layout->data._href) {
      an = buf->layout->data._href->retrieveAnchor(*po);
    }
    if (an == nullptr && buf->layout->data._formitem) {
      an = buf->layout->data._formitem->retrieveAnchor(*po);
    }
    hseq--;
  } while (an == nullptr);

  buf->layout->visual.cursorRow(po->line);
  buf->layout->cursorPos(po->pos);
}

/* go to the nth anchor */
// LINK_N
//"Go to the nth link"
std::shared_ptr<CoroutineState<void>>
nthA(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  auto hl = buf->layout->data._hmarklist;

  // int n = App::instance().searchKeyNum();
  // if (n < 0 || n > (int)hl->size()) {
  //   co_return;
  // }
  //
  // if (buf->layout->empty())
  //   co_return;
  // if (!hl || hl->size() == 0)
  //   co_return;
  //
  // auto po = hl->get(n - 1);
  // Anchor *an = nullptr;
  // if (buf->layout->data._href) {
  //   an = buf->layout->data._href->retrieveAnchor(*po);
  // }
  // if (an == nullptr && buf->layout->data._formitem) {
  //   an = buf->layout->data._formitem->retrieveAnchor(*po);
  // }
  // if (an == nullptr)
  //   co_return;
  //
  // buf->layout->visual.cursorRow(po->line);
  // buf->layout->cursorPos(po->pos);
  co_return;
}

/* go to the next anchor */
// NEXT_LINK
//"Move to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextA(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->_nextA(buf->layout->data.baseURL, 1);
  co_return;
}

/* go to the previous anchor */
// PREV_LINK
//"Move to the previous hyperlink"
std::shared_ptr<CoroutineState<void>>
prevA(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->_prevA(buf->layout->data.baseURL, 1);
  co_return;
}

/* go to the next left anchor */
// NEXT_LEFT
//"Move left to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextL(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextX(-1, 0, 1);
  co_return;
}

/* go to the next left-up anchor */
// NEXT_LEFT_UP
//"Move left or upward to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextLU(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextX(-1, -1, 1);
  co_return;
}

/* go to the next right anchor */
// NEXT_RIGHT
//"Move right to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextR(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextX(1, 0, 1);
  co_return;
}

/* go to the next right-down anchor */
// NEXT_RIGHT_DOWN
//"Move right or downward to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextRD(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextX(1, 1, 1);
  co_return;
}

/* go to the next downward anchor */
// NEXT_DOWN
//"Move downward to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextD(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextY(1, 1);
  co_return;
}

/* go to the next upward anchor */
// NEXT_UP
//"Move upward to the next hyperlink"
std::shared_ptr<CoroutineState<void>>
nextU(const std::shared_ptr<IPlatform> &platform) {
  platform->currentTab()->currentBuffer()->layout->nextY(-1, 1);
  co_return;
}

/* go to the next bufferr */
// NEXT
//"Switch to the next buffer"
std::shared_ptr<CoroutineState<void>>
nextBf(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->forwardBuffer(
      platform->currentTab()->currentBuffer());
  if (!buf) {
    co_return;
  }
  platform->currentTab()->currentBuffer(buf);
}

/* go to the previous bufferr */
// PREV
//"Switch to the previous buffer"
std::shared_ptr<CoroutineState<void>>
prevBf(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer()->backBuffer;
  if (!buf) {
    co_return;
  }
  platform->currentTab()->currentBuffer(buf);
}

/* delete current buffer and back to the previous buffer */
// BACK
//"Close current buffer and return to the one below in stack"
std::shared_ptr<CoroutineState<void>>
backBf(const std::shared_ptr<IPlatform> &platform) {
  // Buffer *buf =
  // platform->currentTab()->currentBuffer()->linkBuffer[LB_N_FRAME];

  if (!platform->currentTab()->currentBuffer()->checkBackBuffer()) {
    if (close_tab_back && App::instance().nTab() >= 1) {
      App::instance().deleteTab(platform->currentTab());
    } else
      App::instance().disp_message("Can't go back...");
    co_return;
  }

  platform->currentTab()->deleteBuffer(platform->currentTab()->currentBuffer());
}

// DELETE_PREVBUF
//"Delete previous buffer (mainly for local CGI-scripts)"
std::shared_ptr<CoroutineState<void>>
deletePrevBuf(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer()->backBuffer;
  if (buf) {
    platform->currentTab()->deleteBuffer(buf);
  }
  co_return;
}

// GOTO
//"Open specified document in a new buffer"
std::shared_ptr<CoroutineState<void>>
goURL(const std::shared_ptr<IPlatform> &platform) {
  auto url = App::instance().searchKeyData();
  if (auto buf = platform->currentTab()->currentBuffer()->goURL0(
          url, "Goto URL: ", false)) {
    platform->currentTab()->pushBuffer(buf);
  }
  co_return;
}

// GOTO_HOME
//"Open home page in a new buffer"
std::shared_ptr<CoroutineState<void>>
goHome(const std::shared_ptr<IPlatform> &platform) {
  const char *url = nullptr;
  if ((url = getenv("HTTP_HOME")) != nullptr ||
      (url = getenv("WWW_HOME")) != nullptr) {
    auto cur_buf = platform->currentTab()->currentBuffer();
    SKIP_BLANKS(url);
    Url p_url(url_quote(url));
    URLHist->push(p_url.to_Str());
    if (auto buf = platform->currentTab()->currentBuffer()->cmd_loadURL(
            url, {}, {}, nullptr)) {
      platform->currentTab()->pushBuffer(buf);
      URLHist->push(buf->res->currentURL.to_Str());
    }
  }
  co_return;
}

// GOTO_RELATIVE
//"Go to relative address"
std::shared_ptr<CoroutineState<void>>
gorURL(const std::shared_ptr<IPlatform> &platform) {
  auto url = App::instance().searchKeyData();
  if (auto buf = platform->currentTab()->currentBuffer()->goURL0(
          url, "Goto relative URL: ", true)) {
    platform->currentTab()->pushBuffer(buf);
  }
  co_return;
}

/* load bookmark */
// BOOKMARK VIEW_BOOKMARK
//"View bookmarks"
std::shared_ptr<CoroutineState<void>>
ldBmark(const std::shared_ptr<IPlatform> &platform) {
  if (auto buf = platform->currentTab()->currentBuffer()->cmd_loadURL(
          BookmarkFile.c_str(), {}, {.no_referer = true}, nullptr)) {
    platform->currentTab()->pushBuffer(buf);
  }
  co_return;
}

/* Add current to bookmark */
// ADD_BOOKMARK
//"Add current page to bookmarks"
std::shared_ptr<CoroutineState<void>>
adBmark(const std::shared_ptr<IPlatform> &platform) {
  std::stringstream ss;
  ss << "mode=panel&cookie=" << (form_quote(localCookie()))
     << "&bmark=" << (form_quote(BookmarkFile)) << "&url="
     << (form_quote(
            platform->currentTab()->currentBuffer()->res->currentURL.to_Str()))
     << "&title="
     << (form_quote(
            platform->currentTab()->currentBuffer()->layout->data.title));
  auto tmp = ss.str();
  auto request = std::make_shared<Form>("", "post", "", "", "");
  request->body = tmp;
  request->length = tmp.size();
  if (auto buf = platform->currentTab()->currentBuffer()->cmd_loadURL(
          "file:///$LIB/" W3MBOOKMARK_CMDNAME, {}, {.no_referer = true},
          request)) {
    platform->currentTab()->pushBuffer(buf);
  }
  co_return;
}

/* option setting */
// OPTIONS
//"Display options setting panel"
std::shared_ptr<CoroutineState<void>>
ldOpt(const std::shared_ptr<IPlatform> &platform) {
  auto html = Option::instance().load_option_panel();
  auto newbuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newbuf);
  co_return;
}

/* set an option */
// SET_OPTION
//"Set option"
std::shared_ptr<CoroutineState<void>>
setOpt(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto opt = App::instance().searchKeyData();
  if (opt.empty() || strchr(opt.c_str(), '=') == nullptr) {
    if (opt.empty()) {
      auto v = Option::instance().get_param_option(opt);
      opt = opt + "=" + v;
    }
    // opt = inputStrHist("Set option: ", opt, TextHist);
    if (opt.empty()) {
      co_return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
}

/* error message list */
// MSGS
//"Display error messages"
std::shared_ptr<CoroutineState<void>>
msgs(const std::shared_ptr<IPlatform> &platform) {
  auto html = App::instance().message_list_panel();
  auto newbuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newbuf);
  co_return;
}

/* page info */
// INFO
//"Display information about the current document"
std::shared_ptr<CoroutineState<void>>
pginfo(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  auto html = buf->page_info_panel();
  auto newBuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newBuf);
  co_return;
}

/* link,anchor,image list */
// LIST
//"Show all URLs referenced"
std::shared_ptr<CoroutineState<void>>
linkLst(const std::shared_ptr<IPlatform> &platform) {
  auto html = link_list_panel(platform->currentTab()->currentBuffer());
  auto newbuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newbuf);
  co_return;
}

/* cookie list */
// COOKIE
//"View cookie list"
std::shared_ptr<CoroutineState<void>>
cooLst(const std::shared_ptr<IPlatform> &platform) {
  auto html = cookie_list_panel();
  auto newbuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newbuf);
  co_return;
}

/* History page */
// HISTORY
//"Show browsing history"
std::shared_ptr<CoroutineState<void>>
ldHist(const std::shared_ptr<IPlatform> &platform) {
  auto html = URLHist->toHtml();
  auto newbuf = Buffer::fromHtml(html);
  platform->currentTab()->pushBuffer(newbuf);
  co_return;
}

/* download HREF link */
// SAVE_LINK
//"Save hyperlink target"
std::shared_ptr<CoroutineState<void>>
svA(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followA(platform);
  // do_download = false;
  co_return;
}

/* download IMG link */
// SAVE_IMAGE
//"Save inline image"
std::shared_ptr<CoroutineState<void>>
svI(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  // do_download = true;
  followI(platform);
  // do_download = false;
  co_return;
}

/* save buffer */
// PRINT SAVE_SCREEN
//"Save rendered document"
std::shared_ptr<CoroutineState<void>>
svBuf(const std::shared_ptr<IPlatform> &platform) {
  const char *qfile = nullptr, *file;
  FILE *f;
  int is_pipe;

// #ifdef _MSC_VER
#if 1
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
    file = Strnew(expandPath(file))->ptr;
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
  platform->currentTab()->currentBuffer()->saveBuffer(f, true);
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
std::shared_ptr<CoroutineState<void>>
svSrc(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->res->sourcefile.empty()) {
    co_return;
  }
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;

  std::string file;
  if (platform->currentTab()->currentBuffer()->res->currentURL.scheme ==
      SCM_LOCAL) {
    file = guess_filename(platform->currentTab()
                              ->currentBuffer()
                              ->res->currentURL.real_file.c_str());
  } else {
    file = platform->currentTab()->currentBuffer()->res->guess_save_name(
        platform->currentTab()->currentBuffer()->res->currentURL.file.c_str());
  }
  doFileCopy(platform->currentTab()->currentBuffer()->res->sourcefile.c_str(),
             file.c_str());
  PermitSaveToPipe = false;
}

/* peek URL */
// PEEK_LINK
//"Show target address"
std::shared_ptr<CoroutineState<void>>
peekURL(const std::shared_ptr<IPlatform> &platform) {
  App::instance().peekURL();
  co_return;
}

// PEEK
//"Show current address"
// std::shared_ptr<CoroutineState<void>> curURL(const std::shared_ptr<IPlatform>
// &platform) {
//   auto url = App::instance().currentUrl();
//   App::instance().disp_message(url.c_str());
//   co_return;
// }

/* view HTML source */
// SOURCE VIEW
//"Toggle between HTML shown or processed"
std::shared_ptr<CoroutineState<void>>
vwSrc(const std::shared_ptr<IPlatform> &platform) {

  if (platform->currentTab()->currentBuffer()->res->type.empty()) {
    co_return;
  }

  if (platform->currentTab()->currentBuffer()->res->sourcefile.empty()) {
    co_return;
  }

  auto buf = platform->currentTab()->currentBuffer()->sourceBuffer();
  platform->currentTab()->pushBuffer(buf);
}

/* reload */
// RELOAD
//"Load current document anew"
std::shared_ptr<CoroutineState<void>>
reload(const std::shared_ptr<IPlatform> &platform) {

  if (platform->currentTab()->currentBuffer()->res->currentURL.scheme ==
          SCM_LOCAL &&
      platform->currentTab()->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't reload stdin");
    co_return;
  }

  auto sbuf = Buffer::create(0);
  *sbuf = *platform->currentTab()->currentBuffer();

  bool multipart = false;
  std::shared_ptr<Form> request;
  if (platform->currentTab()->currentBuffer()->layout->data.form_submit) {
    request = platform->currentTab()
                  ->currentBuffer()
                  ->layout->data.form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      multipart = true;
      platform->currentTab()
          ->currentBuffer()
          ->layout->data.form_submit->query_from_followform_multipart(
              App::instance().pid());
      struct stat st;
      stat(request->body.c_str(), &st);
      request->length = st.st_size;
    }
  } else {
    request = nullptr;
  }

  auto url = platform->currentTab()->currentBuffer()->res->currentURL.to_Str();
  App::instance().message("Reloading...");
  DefaultType = platform->currentTab()->currentBuffer()->res->type;

  auto res =
      loadGeneralFile(url, {}, {.no_referer = true, .no_cache = true}, request);
  DefaultType = nullptr;

  if (multipart) {
    unlink(request->body.c_str());
  }
  if (!res) {
    App::instance().disp_err_message("Can't reload...");
    co_return;
  }

  auto buf = Buffer::create(res);
  // if (buf == NO_BUFFER) {
  //   App::instance().invalidate(platform->currentTab()->currentBuffer(),
  //   B_NORMAL); return;
  // }
  platform->currentTab()->repBuffer(platform->currentTab()->currentBuffer(),
                                    buf);
  if ((buf->res->type.size()) && (sbuf->res->type.size()) &&
      ((buf->res->type == "text/plain" && sbuf->res->is_html_type()) ||
       (buf->res->is_html_type() && sbuf->res->type == "text/plain"))) {
    vwSrc(platform);
    if (platform->currentTab()->currentBuffer() != buf) {
      platform->currentTab()->deleteBuffer(buf);
    }
  }
  platform->currentTab()->currentBuffer()->layout->data.form_submit =
      sbuf->layout->data.form_submit;
  if (platform->currentTab()->currentBuffer()->layout->firstLine()) {
    // platform->currentTab()->currentBuffer()->layout->COPY_BUFROOT_FROM(sbuf->layout);
    platform->currentTab()->currentBuffer()->layout->visual.restorePosition(
        sbuf->layout->visual);
  }
}

/* reshape */
// RESHAPE
//"Re-render document"
std::shared_ptr<CoroutineState<void>>
reshape(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  buf->layout->data.need_reshape = true;
  if (buf->layout->data.need_reshape) {
    auto visual = buf->layout->visual;
    auto body = buf->res->getBody();
    buf->layout->clear();
  }
  co_return;
}

// MARK_URL
//"Turn URL-like strings into hyperlinks"
// std::shared_ptr<CoroutineState<void>> chkURL(const std::shared_ptr<IPlatform>
// &platform) {
//   platform->currentTab()->currentBuffer()->layout->chkURLBuffer();
//   co_return;
// }

// MARK_WORD
//"Turn current word into hyperlink"
// std::shared_ptr<CoroutineState<void>> chkWORD(const
// std::shared_ptr<IPlatform> &platform) {
//   int spos, epos;
//   auto p = platform->currentTab()->currentBuffer()->layout->getCurWord(&spos,
//   &epos); if (p.empty())
//     co_return;
//   platform->currentTab()->currentBuffer()->layout->data.reAnchorWord(
//       platform->currentTab()->currentBuffer()->layout->currentLine(), spos,
//       epos);
// }

/* render frames */
// FRAME
//"Toggle rendering HTML frames"
std::shared_ptr<CoroutineState<void>>
rFrame(const std::shared_ptr<IPlatform> &platform) {
  co_return;
}

// EXTERN
//"Display using an external browser"
std::shared_ptr<CoroutineState<void>>
extbrz(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->res->currentURL.scheme ==
          SCM_LOCAL &&
      platform->currentTab()->currentBuffer()->res->currentURL.file == "-") {
    /* file is std input */
    App::instance().disp_err_message("Can't browse stdin");
    co_return;
  }

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  std::string browser = App::instance().searchKeyData();
  platform->invokeBrowser(
      platform->currentTab()->currentBuffer()->res->currentURL.to_Str(),
      browser);
}

// EXTERN_LINK
//"Display target using an external browser"
std::shared_ptr<CoroutineState<void>>
linkbrz(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;

  auto a = buf->layout->retrieveCurrentAnchor();
  if (a == nullptr)
    co_return;

  Url pu(a->url, buf->layout->data.baseURL);

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  std::string browser = App::instance().searchKeyData();
  platform->invokeBrowser(pu.to_Str(), browser);
}

/* show current line number and number of lines in the entire document */
// LINE_INFO
//"Display current position in document"
std::shared_ptr<CoroutineState<void>>
curlno(const std::shared_ptr<IPlatform> &platform) {
  auto layout = platform->currentTab()->currentBuffer()->layout;
  Line *l = layout->currentLine();
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != nullptr) {
    cur = layout->linenumber(l);
    ;
    col = layout->visual.cursor().col + 1;
    len = l->width();
  }
  if (layout->lastLine())
    all = layout->linenumber(layout->lastLine());

  std::stringstream tmp;
  tmp << "line " << cur << "/" << all << " ("
      << static_cast<int>((double)cur * 100.0 / (double)(all ? all : 1) + 0.5)
      << "%%) col " << col << "/" << len;
  App::instance().disp_message(tmp.str());
  co_return;
}

// VERSION
//"Display the version of w3m"
std::shared_ptr<CoroutineState<void>>
dispVer(const std::shared_ptr<IPlatform> &platform) {
  App::instance().disp_message(std::string("w3m version ") + w3m_version);
  co_return;
}

// WRAP_TOGGLE
//"Toggle wrapping mode in searches"
std::shared_ptr<CoroutineState<void>>
wrapToggle(const std::shared_ptr<IPlatform> &platform) {
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
std::shared_ptr<CoroutineState<void>>
dictword(const std::shared_ptr<IPlatform> &platform) {
  // auto buf = execdict(inputStr("(dictionary)!", ""));
  // platform->currentTab()->pushBuffer(buf);
  // App::instance().invalidate();

  co_return;
}

// DICT_WORD_AT
//"Execute dictionary command for word at cursor"
std::shared_ptr<CoroutineState<void>>
dictwordat(const std::shared_ptr<IPlatform> &platform) {
  auto word = platform->currentTab()->currentBuffer()->layout->getCurWord();
  if (auto res = execdict(word.c_str())) {
    auto buf = Buffer::create(res);
    platform->currentTab()->pushBuffer(buf);
  }
  co_return;
}

// COMMAND
//"Invoke w3m function(s)"
std::shared_ptr<CoroutineState<void>>
execCmd(const std::shared_ptr<IPlatform> &platform) {
  App::instance().doCmd();
  co_return;
}

// ALARM
//"Set alarm"
std::shared_ptr<CoroutineState<void>>
setAlarm(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = App::instance().searchKeyData();
  if (data.empty()) {
    // data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data.empty()) {
      co_return;
    }
  }

  std::string cmd = "";
  auto p = data.c_str();
  int sec = stoi(getWord(&p));
  if (sec > 0) {
    cmd = getWord(&p);
  }
  data = getQWord(&p);
  App::instance().task(sec, cmd, data);
}

// REINIT
//"Reload configuration file"
std::shared_ptr<CoroutineState<void>>
reinit(const std::shared_ptr<IPlatform> &platform) {
  auto resource = App::instance().searchKeyData();

  if (resource.empty()) {
    init_rc();
    sync_with_option();
    load_cookies(rcFile(COOKIE_FILE));
    co_return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
    co_return;
  }

  if (!strcasecmp(resource, "COOKIE")) {
    load_cookies(rcFile(COOKIE_FILE));
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
      std::string("Don't know how to reinitialize '") + resource + "'");
}

// DEFINE_KEY
//"Define a binding between a key stroke combination and a command"
std::shared_ptr<CoroutineState<void>>
defKey(const std::shared_ptr<IPlatform> &platform) {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = App::instance().searchKeyData();
  if (data.empty()) {
    // data = inputStrHist("Key definition: ", "", TextHist);
    if (data.empty()) {
      co_return;
    }
  }
  App::instance().setKeymap(data, -1, true);
}

// NEW_TAB
//"Open a new tab (with current document)"
std::shared_ptr<CoroutineState<void>>
newT(const std::shared_ptr<IPlatform> &platform) {
  App::instance().newTab();
  co_return;
}

// CLOSE_TAB
//"Close tab"
std::shared_ptr<CoroutineState<void>>
closeT(const std::shared_ptr<IPlatform> &platform) {
  if (App::instance().nTab() <= 1)
    co_return;

  std::shared_ptr<TabBuffer> tab = platform->currentTab();
  if (tab)
    App::instance().deleteTab(tab);
}

// NEXT_TAB
//"Switch to the next tab"
std::shared_ptr<CoroutineState<void>>
nextT(const std::shared_ptr<IPlatform> &platform) {
  App::instance().nextTab(1);
  co_return;
}

// PREV_TAB
//"Switch to the previous tab"
std::shared_ptr<CoroutineState<void>>
prevT(const std::shared_ptr<IPlatform> &platform) {
  App::instance().prevTab(1);
  co_return;
}

// TAB_LINK
//"Follow current hyperlink in a new tab"
std::shared_ptr<CoroutineState<void>>
tabA(const std::shared_ptr<IPlatform> &platform) {
  auto [a, buf] =
      co_await platform->currentTab()->currentBuffer()->followAnchor(false);
  if (buf) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_GOTO
//"Open specified document in a new tab"
std::shared_ptr<CoroutineState<void>>
tabURL(const std::shared_ptr<IPlatform> &platform) {
  auto url = App::instance().searchKeyData();
  if (auto buf = platform->currentTab()->currentBuffer()->goURL0(
          url, "Goto URL on new tab: ", false)) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_GOTO_RELATIVE
//"Open relative address in a new tab"
std::shared_ptr<CoroutineState<void>>
tabrURL(const std::shared_ptr<IPlatform> &platform) {
  auto url = App::instance().searchKeyData();
  if (auto buf = platform->currentTab()->currentBuffer()->goURL0(
          url, "Goto relative URL on new tab: ", true)) {
    App::instance().newTab(buf);
  }
  co_return;
}

// TAB_RIGHT
//"Move right along the tab bar"
std::shared_ptr<CoroutineState<void>>
tabR(const std::shared_ptr<IPlatform> &platform) {
  App::instance().tabRight(1);
  co_return;
}

// TAB_LEFT
//"Move left along the tab bar"
std::shared_ptr<CoroutineState<void>>
tabL(const std::shared_ptr<IPlatform> &platform) {
  App::instance().tabLeft(1);
  co_return;
}

/* download panel */
// DOWNLOAD_LIST
//"Display downloads panel"
std::shared_ptr<CoroutineState<void>>
ldDL(const std::shared_ptr<IPlatform> &platform) {
  int replace = false, new_tab = false;

  // if (!FirstDL) {
  //   if (replace) {
  //     if (platform->currentTab()->currentBuffer() ==
  //     platform->currentTab()->firstBuffer &&
  //         platform->currentTab()->currentBuffer()->backBuffer == nullptr) {
  //       if (App::instance().nTab() > 1)
  //         App::instance().deleteTab(platform->currentTab());
  //     } else
  //       platform->currentTab()->deleteBuffer(platform->currentTab()->currentBuffer());
  //   }
  //   co_return;
  // }
  //
  // auto reload = checkDownloadList();
  // auto html = DownloadListBuffer();
  // if (html.empty()) {
  //   co_return;
  // }
  // auto buf = Buffer::fromHtml(html);
  // if (replace) {
  //   //
  //   buf->layout->COPY_BUFROOT_FROM(platform->currentTab()->currentBuffer()->layout);
  //   buf->layout->visual.restorePosition(
  //       platform->currentTab()->currentBuffer()->layout->visual);
  // }
  // if (!replace && open_tab_dl_list) {
  //   App::instance().newTab();
  //   new_tab = true;
  // }
  // platform->currentTab()->pushBuffer(buf);
  // if (replace || new_tab)
  //   deletePrevBuf(platform);
  // if (reload) {
  //   // platform->currentTab()->currentBuffer()->layout->event =
  //   // setAlarmEvent(platform->currentTab()->currentBuffer()->layout->event,
  //   1,
  //   //     AL_IMPLICIT,
  //   //                   FUNCNAME_reload, nullptr);
  // }
  co_return;
}

// UNDO
//"Cancel the last cursor movement"
std::shared_ptr<CoroutineState<void>>
undoPos(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;

  // platform->currentTab()->currentBuffer()->layout->undoPos(PREC_NUM);
}

// REDO
//"Cancel the last undo"
std::shared_ptr<CoroutineState<void>>
redoPos(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;

  // platform->currentTab()->currentBuffer()->layout->redoPos(PREC_NUM);
}

// CURSOR_TOP
//"Move cursor to the top of the screen"
std::shared_ptr<CoroutineState<void>>
cursorTop(const std::shared_ptr<IPlatform> &platform) {
  if (platform->currentTab()->currentBuffer()->layout->empty())
    co_return;

  platform->currentTab()->currentBuffer()->layout->visual.cursorRow(0);
}

// CURSOR_MIDDLE
//"Move cursor to the middle of the screen"
std::shared_ptr<CoroutineState<void>>
cursorMiddle(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = (buf->layout->LINES() - 1) / 2;
  platform->currentTab()->currentBuffer()->layout->visual.cursorMoveRow(
      offsety);
}

// CURSOR_BOTTOM
//"Move cursor to the bottom of the screen"
std::shared_ptr<CoroutineState<void>>
cursorBottom(const std::shared_ptr<IPlatform> &platform) {
  auto buf = platform->currentTab()->currentBuffer();
  if (buf->layout->empty())
    co_return;

  int offsety = buf->layout->LINES() - 1;
  platform->currentTab()->currentBuffer()->layout->visual.cursorMoveRow(
      offsety);
}

/* follow HREF link in the buffer */
std::shared_ptr<CoroutineState<void>>
bufferA(const std::shared_ptr<IPlatform> &platform) {
  on_target = false;
  followA(platform);
  on_target = true;
  co_return;
}
