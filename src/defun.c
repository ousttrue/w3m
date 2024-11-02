#define MAINPROGRAM
#include "defun.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/bufferlist.h"
#include "buffer/display.h"
#include "buffer/document.h"
#include "buffer/downloadlist.h"
#include "buffer/message.h"
#include "buffer/search.h"
#include "buffer/tabbuffer.h"
#include "buffer/w3mbookmark.h"
#include "core.h"
#include "dict.h"
#include "file/file.h"
#include "file/shell.h"
#include "file/tmpfile.h"
#include "fm.h"
#include "func.h"
#include "funcname1.h"
#include "history.h"
#include "html/map.h"
#include "input/content.h"
#include "input/ext_mime.h"
#include "input/ftp.h"
#include "input/http.h"
#include "input/http_cookie.h"
#include "input/https.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "linein.h"
#include "mainloop.h"
#include "os.h"
#include "proto.h"
#include "rc.h"
#include "siteconf.h"
#include "term/scr.h"
#include "term/terms.h"
#include "term/tty.h"
#include "text/myctype.h"
#include "text/regex.h"
#include "text/text.h"
#include "version.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing") { /* do nothing */ }

DEFUN(escmap, ESCMAP, "ESC map") {
  char c = tty_getch();
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map") {
  char c = tty_getch();
  if (IS_DIGIT(c)) {
    escdmap(c);
    return;
  }
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESCB, EscBKeymap);
}

void escdmap(char _c) {
  int d = (int)_c - (int)'0';
  auto c = tty_getch();
  if (IS_DIGIT(c)) {
    d = d * 10 + (int)c - (int)'0';
    c = tty_getch();
  }
  if (c == '~')
    escKeyProc((int)d, K_ESCD, EscDKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap") { multiKeyProc(); }

static Str currentURL(void);

void saveBufferInfo() {
  FILE *fp = fopen(rcFile("bufinfo"), "w");
  if (fp) {
    fprintf(fp, "%s\n", currentURL()->ptr);
    fclose(fp);
  }
}

static void repBuffer(struct Buffer *oldbuf, struct Buffer *buf) {
  Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
  Currentbuf = buf;
}

/*
 * Command functions: These functions are called with a keystroke.
 */
void document_scroll(struct Document *doc, int n) {
  if (doc->firstLine == NULL)
    return;

  struct Line *top = doc->topLine;
  struct Line *cur = doc->currentLine;
  // int tlnum, llnum, diff_n;
  int lnum = cur->linenumber;
  doc->topLine = lineSkip(&doc->viewport, top, doc->lastLine, n, false);
  if (doc->topLine == top) {
    lnum += n;
    if (lnum < doc->topLine->linenumber)
      lnum = doc->topLine->linenumber;
    else if (lnum > doc->lastLine->linenumber)
      lnum = doc->lastLine->linenumber;
  } else {
    int tlnum = doc->topLine->linenumber;
    int llnum = doc->topLine->linenumber + doc->viewport.LINES - 1;
    int diff_n;
    if (nextpage_topline)
      diff_n = 0;
    else
      diff_n = n - (tlnum - top->linenumber);
    if (lnum < tlnum)
      lnum = tlnum + diff_n;
    if (lnum > llnum)
      lnum = llnum + diff_n;
  }
  gotoLine(doc, lnum);
  arrangeLine(doc);
  if (n > 0) {
    if (doc->currentLine->bpos &&
        doc->currentLine->bwidth >=
            doc->viewport.currentColumn + doc->viewport.visualpos)
      cursorDown(doc, 1);
    else {
      while (doc->currentLine->next && doc->currentLine->next->bpos &&
             doc->currentLine->bwidth + doc->currentLine->width <
                 doc->viewport.currentColumn + doc->viewport.visualpos)
        cursorDown0(doc, 1);
    }
  } else {
    if (doc->currentLine->bwidth + doc->currentLine->width <
        doc->viewport.currentColumn + doc->viewport.visualpos)
      cursorUp(doc, 1);
    else {
      while (doc->currentLine->prev && doc->currentLine->bpos &&
             doc->currentLine->bwidth >=
                 doc->viewport.currentColumn + doc->viewport.visualpos)
        cursorUp0(doc, 1);
    }
  }
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page") {
  document_scroll(Currentbuf->document, scrollNum());
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page") {
  document_scroll(Currentbuf->document, -scrollNum());
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page") {
  document_scroll(Currentbuf->document,
                  searchKeyNum() *
                      (Currentbuf->document->viewport.LINES / 2 - 1));
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page") {
  document_scroll(Currentbuf->document,
                  -searchKeyNum() *
                      (Currentbuf->document->viewport.LINES / 2 - 1));
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line") {
  document_scroll(Currentbuf->document, searchKeyNum());
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line") {
  document_scroll(Currentbuf->document, -searchKeyNum());
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line") {
  int offsety;
  if (Currentbuf->document->firstLine == NULL)
    return;
  offsety = Currentbuf->document->viewport.LINES / 2 -
            Currentbuf->document->viewport.cursorY;
  if (offsety != 0) {
    Currentbuf->document->topLine =
        lineSkip(&Currentbuf->document->viewport, Currentbuf->document->topLine,
                 Currentbuf->document->lastLine, -offsety, false);
    arrangeLine(Currentbuf->document);
  }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column") {
  int offsetx;
  if (Currentbuf->document->firstLine == NULL)
    return;
  offsetx = Currentbuf->document->viewport.cursorX -
            Currentbuf->document->viewport.COLS / 2;
  if (offsetx != 0) {
    columnSkip(Currentbuf->document, offsetx);
    arrangeCursor(Currentbuf->document);
  }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew") {
  scr_clear();
  term_clear();
  arrangeCursor(Currentbuf->document);
}

/* Search regular expression forward */

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward") {
  srch(Currentbuf->document, forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward") {
  isrch(Currentbuf->document, forwardSearch, "I-search: ");
}

/* Search regular expression backward */

DEFUN(srchbak, SEARCH_BACK, "Search backward") {
  srch(Currentbuf->document, backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward") {
  isrch(Currentbuf->document, backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward") {
  srch_nxtprv(Currentbuf->document, 0);
}

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward") {
  srch_nxtprv(Currentbuf->document, 1);
}

static void shiftvisualpos(struct Buffer *buf, int shift) {
  struct Line *l = buf->document->currentLine;
  buf->document->viewport.visualpos -= shift;
  if (buf->document->viewport.visualpos - l->bwidth >=
      buf->document->viewport.COLS)
    buf->document->viewport.visualpos =
        l->bwidth + buf->document->viewport.COLS - 1;
  else if (buf->document->viewport.visualpos - l->bwidth < 0)
    buf->document->viewport.visualpos = l->bwidth;
  arrangeLine(buf->document);
  if (buf->document->viewport.visualpos - l->bwidth == -shift &&
      buf->document->viewport.cursorX == 0)
    buf->document->viewport.visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left") {
  int column;

  if (Currentbuf->document->firstLine == NULL)
    return;
  column = Currentbuf->document->viewport.currentColumn;
  columnSkip(Currentbuf->document,
             searchKeyNum() * (-Currentbuf->document->viewport.COLS + 1) + 1);
  shiftvisualpos(Currentbuf,
                 Currentbuf->document->viewport.currentColumn - column);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right") {
  int column;

  if (Currentbuf->document->firstLine == NULL)
    return;
  column = Currentbuf->document->viewport.currentColumn;
  columnSkip(Currentbuf->document,
             searchKeyNum() * (Currentbuf->document->viewport.COLS - 1) - 1);
  shiftvisualpos(Currentbuf,
                 Currentbuf->document->viewport.currentColumn - column);
}

DEFUN(col1R, RIGHT, "Shift screen one column right") {
  struct Buffer *buf = Currentbuf;
  struct Line *l = buf->document->currentLine;
  int j, column, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    column = buf->document->viewport.currentColumn;
    columnSkip(Currentbuf->document, 1);
    if (column == buf->document->viewport.currentColumn)
      break;
    shiftvisualpos(Currentbuf, 1);
  }
}

DEFUN(col1L, LEFT, "Shift screen one column left") {
  struct Buffer *buf = Currentbuf;
  struct Line *l = buf->document->currentLine;
  int j, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    if (buf->document->viewport.currentColumn == 0)
      break;
    columnSkip(Currentbuf->document, -1);
    shiftvisualpos(Currentbuf, -1);
  }
}

DEFUN(setEnv, SETENV, "Set environment variable") {
  clearKeyData();
  const char *env = searchKeyData();
  if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
    if (env != NULL && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    env = inputStrHist(Currentbuf->document, "Set environ: ", env, TextHist);
    if (env == NULL || *env == '\0') {
      return;
    }
  }
  char *value;
  if ((value = strchr(env, '=')) != NULL && value > env) {
    auto var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output") {
  // CurrentKeyData = NULL; /* not allowed in w3m-control: */
  // const char *cmd = searchKeyData();
  // if (cmd == NULL || *cmd == '\0') {
  //   cmd = inputLineHist(Currentbuf->document, "(read shell)!", "",
  //   IN_COMMAND,
  //                       ShellHist);
  // }
  // if (cmd == NULL || *cmd == '\0') {
  //   displayBuffer(Currentbuf, B_NORMAL);
  //   return;
  // }
  // // auto prevtrap = mySignal(SIGINT, intTrap);
  // tty_crmode();
  // auto buf = getshell(cmd);
  // // mySignal(SIGINT, prevtrap);
  // tty_raw();
  // if (buf == NULL) {
  //   disp_message("Execution failed", true);
  //   return;
  // } else {
  //   buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
  //   if (buf->type == NULL)
  //     buf->type = "text/plain";
  //   pushBuffer(buf);
  // }
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output") {
  clearKeyData(); /* not allowed in w3m-control: */
  const char *cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist(Currentbuf->document, "(exec shell)!", "", IN_COMMAND,
                        ShellHist);
  }
  if (cmd != NULL && *cmd != '\0') {
    term_fmTerm();
    printf("\n");
    system(cmd);
    printf("\n[Hit any key]");
    fflush(stdout);
    term_fmInit();
    tty_getch();
  }
}

static void cmd_loadfile(const char *fn) {
  auto content = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
  if (!content) {
    const char *emsg = Sprintf("%s not found", fn)->ptr;
    message_push(emsg);
    return;
  }
  pushContent(CurrentTab, content);
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer") {
  const char *fn = searchKeyData();
  if (fn == NULL || *fn == '\0') {
    fn = inputFilenameHist(Currentbuf->document, "(Load)Filename? ", NULL,
                           LoadHist);
  }
  if (fn == NULL || *fn == '\0') {
    return;
  }
  cmd_loadfile(fn);
}

/* Load help file */
#define HELP_CGI "w3mhelp"
DEFUN(ldhelp, HELP, "Show help panel") {
  auto lang = AcceptLang;
  int n = strcspn(lang, ";, \t");
  Str tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  auto content = loadGeneralFile(tmp->ptr, NULL, NO_REFERER, false, NULL);
  pushContent(CurrentTab, content);
}

/* Move cursor left */
static void _movL(int n) {
  int m = searchKeyNum();
  if (Currentbuf->document->firstLine == NULL)
    return;
  for (int i = 0; i < m; i++)
    cursorLeft(Currentbuf->document, n);
}

DEFUN(movL, MOVE_LEFT, "Cursor left") {
  _movL(Currentbuf->document->viewport.COLS / 2);
}

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide") { _movL(1); }

/* Move cursor downward */
static void _movD(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document->firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorDown(Currentbuf->document, n);
}

DEFUN(movD, MOVE_DOWN, "Cursor down") {
  _movD((Currentbuf->document->viewport.LINES + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide") { _movD(1); }

/* move cursor upward */
static void _movU(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document->firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorUp(Currentbuf->document, n);
}

DEFUN(movU, MOVE_UP, "Cursor up") {
  _movU((Currentbuf->document->viewport.LINES + 1) / 2);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide") { _movU(1); }

/* Move cursor right */
static void _movR(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document->firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorRight(Currentbuf->document, n);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right") {
  _movR(Currentbuf->document->viewport.COLS / 2);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide") {
  _movR(1);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

DEFUN(movLW, PREV_WORD, "Move to the previous word") {
  if (Currentbuf->document->firstLine == NULL)
    return;

  int n = searchKeyNum();
  for (int i = 0; i < n; i++) {
    auto pline = Currentbuf->document->currentLine;
    int ppos = Currentbuf->document->viewport.pos;

    if (prev_nonnull_line(Currentbuf->document,
                          Currentbuf->document->currentLine) < 0)
      goto end;

    while (1) {
      auto l = Currentbuf->document->currentLine;
      auto lb = l->lineBuf;
      while (Currentbuf->document->viewport.pos > 0) {
        int tmp = Currentbuf->document->viewport.pos;
        prevChar(&tmp, l);
        if (is_wordchar(lb[tmp]))
          break;
        Currentbuf->document->viewport.pos = tmp;
      }
      if (Currentbuf->document->viewport.pos > 0)
        break;
      if (prev_nonnull_line(Currentbuf->document,
                            Currentbuf->document->currentLine->prev) < 0) {
        Currentbuf->document->currentLine = pline;
        Currentbuf->document->viewport.pos = ppos;
        goto end;
      }
      Currentbuf->document->viewport.pos =
          Currentbuf->document->currentLine->len;
    }

    auto l = Currentbuf->document->currentLine;
    auto lb = l->lineBuf;
    while (Currentbuf->document->viewport.pos > 0) {
      int tmp = Currentbuf->document->viewport.pos;
      prevChar(&tmp, l);
      if (!is_wordchar(lb[tmp]))
        break;
      Currentbuf->document->viewport.pos = tmp;
    }
  }
end:
  arrangeCursor(Currentbuf->document);
}

static int next_nonnull_line(struct Line *line) {
  struct Line *l;

  for (l = line; l != NULL && l->len == 0; l = l->next)
    ;

  if (l == NULL || l->len == 0)
    return -1;

  Currentbuf->document->currentLine = l;
  if (l != line)
    Currentbuf->document->viewport.pos = 0;
  return 0;
}

DEFUN(movRW, NEXT_WORD, "Move to the next word") {
  char *lb;
  struct Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->document->firstLine == NULL)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->document->currentLine;
    ppos = Currentbuf->document->viewport.pos;

    if (next_nonnull_line(Currentbuf->document->currentLine) < 0)
      goto end;

    l = Currentbuf->document->currentLine;
    lb = l->lineBuf;
    while (Currentbuf->document->viewport.pos < l->len &&
           is_wordchar(lb[Currentbuf->document->viewport.pos]))
      nextChar(&Currentbuf->document->viewport.pos, l);

    while (1) {
      while (Currentbuf->document->viewport.pos < l->len &&
             !is_wordchar(lb[Currentbuf->document->viewport.pos]))
        nextChar(&Currentbuf->document->viewport.pos, l);
      if (Currentbuf->document->viewport.pos < l->len)
        break;
      if (next_nonnull_line(Currentbuf->document->currentLine->next) < 0) {
        Currentbuf->document->currentLine = pline;
        Currentbuf->document->viewport.pos = ppos;
        goto end;
      }
      Currentbuf->document->viewport.pos = 0;
      l = Currentbuf->document->currentLine;
      lb = l->lineBuf;
    }
  }
end:
  arrangeCursor(Currentbuf->document);
}

static void _quitfm(int confirm) {
  const char *ans = "y";
  if (checkDownloadList())
    ans = inputChar(Currentbuf->document, "Download process retains. "
                                          "Do you want to exit w3m? (y/n)");
  else if (confirm)
    ans = inputChar(Currentbuf->document, "Do you want to exit w3m? (y/n)");

  if (!(ans && TOLOWER(*ans) == 'y')) {
    return;
  }

  term_title(""); /* XXX */
  term_fmTerm();
  save_cookies();
  if (UseHistory && SaveURLHist)
    saveHistory(URLHist, URLHistSize);
  w3m_exit(0);
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit at once") { _quitfm(false); }

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request") {
  _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel") {
  auto ok = false;
  do {
    char cmd;
    auto buf = selectBuffer(Firstbuf, Currentbuf, term_size(), &cmd);
    term_refresh();
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
      delBuffer(CurrentTab, buf);
      // if (Firstbuf == NULL) {
      //   /* No more buffer */
      //   Firstbuf = nullBuffer();
      //   Currentbuf = Firstbuf;
      // }
      break;

    case 'q':
      _quitfm(true);
      break;

    case 'Q':
      _quitfm(false);
      break;
    }
  } while (!ok);

  for (auto buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
    if (buf == Currentbuf)
      continue;
    if (clear_buffer)
      tmpClearBuffer(buf->document);
  }
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background") { term_suspend(); }

DEFUN(goLine, GOTO_LINE, "Go to the specified line") {
  _goLine(Currentbuf->document, goLineStr());
}

DEFUN(goLineF, BEGIN, "Go to the first line") {
  _goLine(Currentbuf->document, "^");
}

DEFUN(goLineL, END, "Go to the last line") {
  _goLine(Currentbuf->document, "$");
}

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  while (Currentbuf->document->currentLine->prev &&
         Currentbuf->document->currentLine->bpos)
    cursorUp0(Currentbuf->document, 1);
  Currentbuf->document->viewport.pos = 0;
  arrangeCursor(Currentbuf->document);
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  while (Currentbuf->document->currentLine->next &&
         Currentbuf->document->currentLine->next->bpos)
    cursorDown0(Currentbuf->document, 1);
  Currentbuf->document->viewport.pos =
      Currentbuf->document->currentLine->len - 1;
  arrangeCursor(Currentbuf->document);
}

static int cur_real_linenumber(struct Buffer *buf) {
  struct Line *l, *cur = buf->document->currentLine;
  int n;

  if (!cur)
    return 1;
  n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (l = buf->document->firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source") {
  auto fn = Currentbuf->content->filename;
  if (!fn ||
      (Currentbuf->content->content_type == NULL &&
       Currentbuf->edit == NULL) || /* Reading shell */
      Currentbuf->content->real_scheme != SCM_LOCAL ||
      !strcmp(Currentbuf->content->url.file, "-") || /* file is std input  */
      Currentbuf->document->bufferprop & BP_FRAME) { /* Frame */
    message_push("Can't edit other than local file");
    return;
  }

  const char *cmd;
  if (Currentbuf->edit) {
    cmd = Currentbuf->edit;
  } else
    cmd =
        myEditor(Editor, shell_quote(fn), cur_real_linenumber(Currentbuf))->ptr;
  term_fmTerm();
  system(cmd);
  term_fmInit();

  reload(current);
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document") {
  auto tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
  auto f = fopen(tmpf, "w");
  if (f == NULL) {
    message_push(Sprintf("Can't open %s", tmpf)->ptr);
    return;
  }
  saveBuffer(Currentbuf, f, true);
  fclose(f);
  term_fmTerm();
  system(myEditor(Editor, shell_quote(tmpf), cur_real_linenumber(Currentbuf))
             ->ptr);
  term_fmInit();
  unlink(tmpf);
}

static void gotoLabel(struct Buffer *buf, const char *label) {
  auto al = searchURLLabel(buf->document, label);
  if (!al) {
    message_push(Sprintf("%s is not found", label)->ptr);
    return;
  }

  pushContent(CurrentTab, buf->content);
  gotoLine(Currentbuf->document, al->start.line);
  if (label_topline)
    Currentbuf->document->topLine =
        lineSkip(&Currentbuf->document->viewport, Currentbuf->document->topLine,
                 Currentbuf->document->lastLine,
                 Currentbuf->document->currentLine->linenumber -
                     Currentbuf->document->topLine->linenumber,
                 false);
  Currentbuf->document->viewport.pos = al->start.pos;
  arrangeCursor(Currentbuf->document);
  return;
}

struct Content *_followA(struct Current current) {
  if (Currentbuf->document->firstLine == NULL)
    return nullptr;

  auto a = retrieveCurrentMap(Currentbuf->document);
  if (a) {
    return _followForm(Currentbuf, false, current);
  }

  a = retrieveCurrentAnchor(Currentbuf->document);
  if (a == NULL) {
    return _followForm(Currentbuf, false, current);
  }

  if (*a->url == '#') { /* index within this buffer */
    gotoLabel(Currentbuf, a->url + 1);
    return nullptr;
  }

  auto u = parseURL2(a->url, baseURL(Currentbuf));
  if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->content->url)) ==
      0) {
    /* index within this buffer */
    if (u.label) {
      gotoLabel(Currentbuf, u.label);
      return nullptr;
    }
  }
  if (handleMailto(a->url))
    return nullptr;
  auto url = a->url;

  return loadLink(Currentbuf, url, a->target, a->referer, NULL);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer") {
  auto content = _followA(current);
  pushContent(CurrentTab, content);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer") {
  if (Currentbuf->document->firstLine == NULL)
    return;

  auto a = retrieveCurrentImg(Currentbuf->document);
  if (!a)
    return;

  scr_message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  term_refresh();
  auto content = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
  if (!content) {
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    message_push(emsg);
    return;
  }
  pushContent(CurrentTab, content);
}

#define conv_form_encoding(val, fi, buf) (val)

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form") {
  auto content = _followForm(Currentbuf, true, current);
  pushContent(CurrentTab, content);
}

/* process form */
void followForm(struct Current current) {
  auto content = _followForm(Currentbuf, false, current);
  pushContent(CurrentTab, content);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink") {

  if (Currentbuf->document->firstLine == NULL)
    return;

  struct HmarkerList *hl = Currentbuf->document->hmarklist;
  if (!hl || hl->nmark == 0)
    return;

  int hseq = getHseq(hl->nmark);
  struct BufferPoint *po;
  struct Anchor *an;
  do {
    if (hseq >= hl->nmark)
      return;
    po = hl->marks + hseq;
    an = retrieveAnchor(Currentbuf->document->href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->document->formitem, po->line, po->pos);
    hseq++;
  } while (an == NULL);

  gotoLine(Currentbuf->document, po->line);
  Currentbuf->document->viewport.pos = po->pos;
  arrangeCursor(Currentbuf->document);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink") {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;
  struct BufferPoint *po;
  struct Anchor *an;

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  int hseq = getLastHseq(hl->nmark);
  do {
    if (hseq < 0)
      return;
    po = hl->marks + hseq;
    an = retrieveAnchor(Currentbuf->document->href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->document->formitem, po->line, po->pos);
    hseq--;
  } while (an == NULL);

  gotoLine(Currentbuf->document, po->line);
  Currentbuf->document->viewport.pos = po->pos;
  arrangeCursor(Currentbuf->document);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link") {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;
  struct BufferPoint *po;
  struct Anchor *an;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  po = hl->marks + n - 1;
  an = retrieveAnchor(Currentbuf->document->href, po->line, po->pos);
  if (an == NULL)
    an = retrieveAnchor(Currentbuf->document->formitem, po->line, po->pos);
  if (an == NULL)
    return;

  gotoLine(Currentbuf->document, po->line);
  Currentbuf->document->viewport.pos = po->pos;
  arrangeCursor(Currentbuf->document);
}

/* go to the next [visited] anchor */
static void _nextA(bool visited) {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(Currentbuf->document);
  if (visited != true && an == NULL)
    an = retrieveCurrentForm(Currentbuf->document);

  auto y = Currentbuf->document->currentLine->linenumber;
  auto x = Currentbuf->document->viewport.pos;

  int n = searchKeyNum();
  if (visited == true) {
    n = hl->nmark;
  }

  for (auto i = 0; i < n; i++) {
    auto pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= hl->nmark) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        auto po = &hl->marks[hseq];
        an = retrieveAnchor(Currentbuf->document->href, po->line, po->pos);
        if (visited != true && an == NULL)
          an =
              retrieveAnchor(Currentbuf->document->formitem, po->line, po->pos);
        hseq++;
        if (visited == true && an) {
          auto url = parseURL2(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
            goto _end;
          }
        }
      } while (an == NULL || an == pan);
    } else {
      an = closest_next_anchor(Currentbuf->document->href, NULL, x, y);
      if (visited != true)
        an = closest_next_anchor(Currentbuf->document->formitem, an, x, y);
      if (an == NULL) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true) {
        auto url = parseURL2(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == NULL || an->hseq < 0)
    return;
  auto po = &hl->marks[an->hseq];
  gotoLine(Currentbuf->document, po->line);
  Currentbuf->document->viewport.pos = po->pos;
  arrangeCursor(Currentbuf->document);
}
/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink") { _nextA(false); }

/* go to the previous anchor */
static void _prevA(int visited) {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  auto an = retrieveCurrentAnchor(Currentbuf->document);
  if (visited != true && an == NULL)
    an = retrieveCurrentForm(Currentbuf->document);

  auto y = Currentbuf->document->currentLine->linenumber;
  auto x = Currentbuf->document->viewport.pos;

  int n = searchKeyNum();
  if (visited == true) {
    n = hl->nmark;
  }

  for (int i = 0; i < n; i++) {
    auto pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        auto po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->document->href, po->line, po->pos);
        if (visited != true && an == NULL)
          an =
              retrieveAnchor(Currentbuf->document->formitem, po->line, po->pos);
        hseq--;
        if (visited == true && an) {
          auto url = parseURL2(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
            goto _end;
          }
        }
      } while (an == NULL || an == pan);
    } else {
      an = closest_prev_anchor(Currentbuf->document->href, NULL, x, y);
      if (visited != true)
        an = closest_prev_anchor(Currentbuf->document->formitem, an, x, y);
      if (an == NULL) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true && an) {
        auto url = parseURL2(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == NULL || an->hseq < 0)
    return;
  auto po = hl->marks + an->hseq;
  gotoLine(Currentbuf->document, po->line);
  Currentbuf->document->viewport.pos = po->pos;
  arrangeCursor(Currentbuf->document);
}

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

/* go to the next left/right anchor */
static void nextX(int d, int dy) {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;
  struct Anchor *an, *pan;
  struct Line *l;
  int i, x, y, n = searchKeyNum();

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf->document);
  if (an == NULL)
    an = retrieveCurrentForm(Currentbuf->document);

  l = Currentbuf->document->currentLine;
  x = Currentbuf->document->viewport.pos;
  y = l->linenumber;
  pan = NULL;
  for (i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = NULL;
    while (1) {
      for (; x >= 0 && x < l->len; x += d) {
        an = retrieveAnchor(Currentbuf->document->href, y, x);
        if (!an)
          an = retrieveAnchor(Currentbuf->document->formitem, y, x);
        if (an) {
          pan = an;
          break;
        }
      }
      if (!dy || an)
        break;
      l = (dy > 0) ? l->next : l->prev;
      if (!l)
        break;
      x = (d > 0) ? 0 : l->len - 1;
      y = l->linenumber;
    }
    if (!an)
      break;
  }

  if (pan == NULL)
    return;
  gotoLine(Currentbuf->document, y);
  Currentbuf->document->viewport.pos = pan->start.pos;
  arrangeCursor(Currentbuf->document);
}

/* go to the next downward/upward anchor */
static void nextY(int d) {
  struct HmarkerList *hl = Currentbuf->document->hmarklist;
  struct Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  int hseq;

  if (Currentbuf->document->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf->document);
  if (an == NULL)
    an = retrieveCurrentForm(Currentbuf->document);

  x = Currentbuf->document->viewport.pos;
  y = Currentbuf->document->currentLine->linenumber + d;
  pan = NULL;
  hseq = -1;
  for (i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = NULL;
    for (; y >= 0 && y <= Currentbuf->document->lastLine->linenumber; y += d) {
      an = retrieveAnchor(Currentbuf->document->href, y, x);
      if (!an)
        an = retrieveAnchor(Currentbuf->document->formitem, y, x);
      if (an && hseq != abs(an->hseq)) {
        pan = an;
        break;
      }
    }
    if (!an)
      break;
  }

  if (pan == NULL)
    return;
  gotoLine(Currentbuf->document, pan->start.line);
  arrangeLine(Currentbuf->document);
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
  for (int i = 0; i < precNum(); i++) {
    auto buf = prevBuffer(Firstbuf, Currentbuf);
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    Currentbuf = buf;
  }
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer") {
  for (int i = 0; i < precNum(); i++) {
    auto buf = Currentbuf->nextBuffer;
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    Currentbuf = buf;
  }
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK,
      "Close current buffer and return to the one below in stack") {
  _backBf(CurrentTab);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local CGI-scripts)") {
  // nextBuffer が history prev であることに注意！
  struct Buffer *buf = Currentbuf->nextBuffer;
  if (buf) {
    delBuffer(CurrentTab, buf);
  }
}

/* go to specified URL */
static struct Content *goURL0(char *prompt, int relative) {
  const char *referer;
  struct Url *current;
  struct Buffer *cur_buf = Currentbuf;
  const int *no_referer_ptr;

  auto url = searchKeyData();
  if (url == NULL) {
    struct Hist *hist = copyHist(URLHist);
    auto current = baseURL(Currentbuf);
    if (current) {
      char *c_url = parsedURL2Str(current)->ptr;
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url);
      else
        pushHist(hist, c_url);
    }
    auto a = retrieveCurrentAnchor(Currentbuf->document);
    if (a) {
      auto tmp = parseURL2(a->url, current);
      auto a_url = parsedURL2Str(&tmp)->ptr;
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url);
      else
        pushHist(hist, a_url);
    }
    url = inputLineHist(Currentbuf->document, prompt, url, IN_URL, hist);
    if (url != NULL)
      SKIP_BLANKS(url);
  }
  if (relative) {
    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->content->url);
    current = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || current == NULL ||
        current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI ||
        current->scheme == SCM_DATA)
      referer = NO_REFERER;
    else
      referer = parsedURL2RefererStr(&Currentbuf->content->url)->ptr;
    url = url_quote(url);
  } else {
    current = NULL;
    referer = NULL;
    url = url_quote(url);
  }
  if (url == NULL || *url == '\0') {
    return nullptr;
  }
  if (*url == '#') {
    gotoLabel(Currentbuf, url + 1);
    return nullptr;
  }

  auto p_url = parseURL2(url, current);
  pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
  auto content = loadGeneralFile(url, current, referer, false, NULL);
  if (content) { /* success */
    pushHashHist(URLHist, parsedURL2Str(&content->url)->ptr);
  }
  return content;
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer") {
  goURL0("Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer") {
  const char *url;
  if ((url = getenv("HTTP_HOME")) != NULL ||
      (url = getenv("WWW_HOME")) != NULL) {
    struct Buffer *cur_buf = Currentbuf;
    SKIP_BLANKS(url);
    url = url_quote(url);
    auto p_url = parseURL2(url, NULL);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    auto content = loadGeneralFile(url, NULL, NULL, false, NULL);
    if (content) { /* success */
      pushHashHist(URLHist, parsedURL2Str(&content->url)->ptr);
      pushContent(CurrentTab, content);
    }
  }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address") {
  goURL0("Goto relative URL: ", true);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks") {
  auto content = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, false, NULL);
  pushContent(CurrentTab, content);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks") {
  auto tmp =
      Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
              (Str_form_quote(localCookie()))->ptr,
              (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
              (Str_form_quote(parsedURL2Str(&Currentbuf->content->url)))->ptr,
              (Str_form_quote(Strnew_charp(Currentbuf->document->title)))->ptr);
  auto form = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
  form->body = tmp->ptr;
  form->length = tmp->length;
  auto content = loadGeneralFile("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL,
                                 NO_REFERER, false, form);
  pushContent(CurrentTab, content);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel") {
  auto html = load_option_panel();

  struct Url url;
  // return renderHTML(INIT_BUFFER_WIDTH, src->ptr, url, CHARSET_UTF8);
  auto content =
      newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
  pushContent(CurrentTab, content);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option") {
  clearKeyData(); /* not allowed in w3m-control: */
  const char *opt = searchKeyData();
  if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
    if (opt != NULL && *opt != '\0') {
      auto v = get_param_option(opt);
      opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
    }
    opt = inputStrHist(Currentbuf->document, "Set option: ", opt, TextHist);
    if (opt == NULL || *opt == '\0') {
      return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages") {
  auto html = message_list_panel();
  struct Url url;
  // auto doc = renderHTML(INIT_BUFFER_WIDTH, html, url, CHARSET_UTF8);
  auto content =
      newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
  pushContent(CurrentTab, content);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document") {
  auto html = page_info_panel(Currentbuf);
  struct Url url;
  auto content =
      newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
  pushContent(CurrentTab, content);
}

void follow_map(struct InternalAction *arg, struct Current) {
  auto name = tag_get_value(arg, "link");

#if defined(MENU_MAP) || defined(USE_IMAGE)
  auto an = retrieveCurrentImg(Currentbuf->document);
  auto x = Currentbuf->document->viewport.cursorX +
           Currentbuf->document->viewport.rootX;
  auto y = Currentbuf->document->viewport.cursorY +
           Currentbuf->document->viewport.rootY;
  auto a = follow_map_menu(Currentbuf->document, name, an, x, y);
  if (a == NULL || a->url == NULL || *(a->url) == '\0') {
#endif
    auto html = follow_map_panel(Currentbuf, name);
    if (html) {
      struct Url url;
      auto content =
          newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
      pushContent(CurrentTab, content);
    }

#if defined(MENU_MAP) || defined(USE_IMAGE)
    return;
  }
  if (*(a->url) == '#') {
    gotoLabel(Currentbuf, a->url + 1);
    return;
  }
  auto p_url = parseURL2(a->url, baseURL(Currentbuf));
  pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);

  auto content = loadGeneralFile(a->url, baseURL(Currentbuf),
                                 parsedURL2Str(&Currentbuf->content->url)->ptr,
                                 false, NULL);

  pushCheckTarget(CurrentTab, a->target, content, true);
#endif
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced") {
  auto html = link_list_panel(Currentbuf);
  if (html) {
    struct Url url;
    auto content =
        newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
    pushContent(CurrentTab, content);
  }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list") {
  auto html = cookie_list_panel();
  if (html) {
    struct Url url;
    auto content =
        newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
    pushContent(CurrentTab, content);
  }
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history") {
  auto html = historyDocument(URLHist);
  if (html) {
    struct Url url;
    auto content =
        newContent(url, html->ptr, html->length, "text/html", CHARSET_UTF8);
    pushContent(CurrentTab, content);
  }
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target") {
  // CurrentKeyData = NULL; /* not allowed in w3m-control: */
  // do_download = true;
  // followA();
  // do_download = false;
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image") {
  // CurrentKeyData = NULL; /* not allowed in w3m-control: */
  // do_download = true;
  // followI();
  // do_download = false;
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document") {
  const char *qfile = NULL;

  clearKeyData(); /* not allowed in w3m-control: */
  auto file = searchKeyData();
  if (file == NULL || *file == '\0') {
    qfile = inputLineHist(Currentbuf->document, "Save buffer to: ", NULL,
                          IN_COMMAND, SaveHist);
    if (qfile == NULL || *qfile == '\0') {
      return;
    }
  }
  file = qfile ? qfile : file;

  FILE *f = nullptr;
  bool is_pipe = false;
  if (*file == '|') {
    is_pipe = true;
    f = popen(file + 1, "w");
  } else {
    if (qfile) {
      file = unescape_spaces(Strnew_charp(qfile))->ptr;
    }
    file = expandPath(file);
    if (checkOverWrite(file) < 0) {
      return;
    }
    f = fopen(file, "w");
    is_pipe = false;
  }
  if (f == NULL) {
    const char *emsg = Sprintf("Can't open %s", file)->ptr;
    message_push(emsg);
    return;
  }
  saveBuffer(Currentbuf, f, true);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source") {
  if (Currentbuf->content->sourcefile == NULL)
    return;

  clearKeyData(); /* not allowed in w3m-control: */
  PermitSaveToPipe = true;
  const char *file;
  if (Currentbuf->content->real_scheme == SCM_LOCAL)
    file = guess_save_name(NULL, Currentbuf->content->url.real_file);
  else
    file = guess_save_name(Currentbuf->http_response,
                           Currentbuf->content->url.file);
  doFileCopy(Currentbuf->content->sourcefile, file);
  PermitSaveToPipe = false;
}

static void _peekURL(int only_img) {
  //   static Str s = NULL;
  //   static int offset = 0;
  //
  //   if (Currentbuf->document->firstLine == NULL)
  //     return;
  //   if (CurrentKey == prev_key && s != NULL) {
  //     if (s->length - offset >= COLS)
  //       offset++;
  //     else if (s->length <= offset) /* bug ? */
  //       offset = 0;
  //     goto disp;
  //   } else {
  //     offset = 0;
  //   }
  //   s = NULL;
  //   auto a = (only_img ? NULL : retrieveCurrentAnchor(Currentbuf->document));
  //   if (a == NULL) {
  //     a = (only_img ? NULL : retrieveCurrentForm(Currentbuf->document));
  //     if (a == NULL) {
  //       a = retrieveCurrentImg(Currentbuf->document);
  //       if (a == NULL)
  //         return;
  //     } else
  //       s = Strnew_charp(form2str((struct FormItemList *)a->url));
  //   }
  //   if (s == NULL) {
  //     auto pu = parseURL2(a->url, baseURL(Currentbuf->document));
  //     s = parsedURL2Str(&pu);
  //   }
  //   if (DecodeURL)
  //     s = Strnew_charp(url_decode0(s->ptr));
  // disp:
  //   int n = searchKeyNum();
  //   if (n > 1 && s->length > (n - 1) * (COLS - 1))
  //     offset = (n - 1) * (COLS - 1);
  //   message_push(&s->ptr[offset]);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address") { _peekURL(0); }

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address") { _peekURL(1); }

/* show current URL */
static Str currentURL(void) {
  if (Currentbuf->document) {
    if (Currentbuf->document->bufferprop & BP_INTERNAL)
      return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->content->url);
  }
  return Strnew_size(0);
}

DEFUN(curURL, PEEK, "Show current address") {
  // static Str s = NULL;
  // static int offset = 0, n;
  //
  // if (Currentbuf->document->bufferprop & BP_INTERNAL)
  //   return;
  // if (CurrentKey == prev_key && s != NULL) {
  //   if (s->length - offset >= COLS)
  //     offset++;
  //   else if (s->length <= offset) /* bug ? */
  //     offset = 0;
  // } else {
  //   offset = 0;
  //   s = currentURL();
  //   if (DecodeURL)
  //     s = Strnew_charp(url_decode0(s->ptr));
  // }
  // n = searchKeyNum();
  // if (n > 1 && s->length > (n - 1) * (COLS - 1))
  //   offset = (n - 1) * (COLS - 1);
  // message_push(&s->ptr[offset]);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed") {
  // struct Buffer *buf;
  //
  // if (Currentbuf->document->type == NULL ||
  //     Currentbuf->document->bufferprop & BP_FRAME)
  //   return;
  // if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL ||
  //     (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
  //   Currentbuf = buf;
  //   return;
  // }
  // if (Currentbuf->document->sourcefile == NULL) {
  //   return;
  // }
  //
  // buf = newBuffer();
  //
  // if (is_html_type(Currentbuf->document->type)) {
  //   buf->document->type = "text/plain";
  //   // if (Currentbuf->document->real_type &&
  //   //     is_html_type(Currentbuf->document->real_type))
  //   //   buf->document->real_type = "text/plain";
  //   // else
  //   //   buf->document->real_type = Currentbuf->document->real_type;
  //   buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
  //   buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
  //   Currentbuf->linkBuffer[LB_SOURCE] = buf;
  // } else if (!strcasecmp(Currentbuf->document->type, "text/plain")) {
  //   buf->document->type = "text/html";
  //   // if (Currentbuf->document->real_type &&
  //   //     !strcasecmp(Currentbuf->document->real_type, "text/plain"))
  //   //   buf->document->real_type = "text/html";
  //   // else
  //   //   buf->document->real_type = Currentbuf->document->real_type;
  //   buf->buffername = Sprintf("HTML view of %s",
  //   Currentbuf->buffername)->ptr; buf->linkBuffer[LB_SOURCE] = Currentbuf;
  //   Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
  // } else {
  //   return;
  // }
  // buf->content->url = Currentbuf->document->url;
  // buf->document->real_scheme = Currentbuf->document->real_scheme;
  // buf->document->filename = Currentbuf->document->filename;
  // buf->document->sourcefile = Currentbuf->document->sourcefile;
  // buf->clone = Currentbuf->clone;
  // (*buf->clone)++;
  //
  // buf->document->need_reshape = true;
  // buf->document =
  //     reshapeBuffer(buf->document, buf->http_response->content_charset);
  // pushBuffer(CurrentTab, buf);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew") {
  if (Currentbuf->document->bufferprop & BP_INTERNAL) {
    if (!strcmp(Currentbuf->document->title, DOWNLOAD_LIST_TITLE)) {
      ldDL(current);
      return;
    }
    message_push("Can't reload...");
    return;
  }

  if (Currentbuf->content->url.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->content->url.file, "-")) {
    /* file is std input */
    message_push("Can't reload stdin");
    return;
  }
  struct Document sbuf;
  copyBuffer(&sbuf, Currentbuf->document);

  struct Buffer *fbuf = NULL;
  Str url;
  struct FormList *form;
  bool multipart = 0;
  if (Currentbuf->document->form_submit) {
    form = Currentbuf->document->form_submit->parent;
    if (form->method == FORM_METHOD_POST &&
        form->enctype == FORM_ENCTYPE_MULTIPART) {
      Str query;
      struct stat st;
      multipart = 1;
      query_from_followform(&query, Currentbuf->document->form_submit,
                            multipart);
      stat(form->body, &st);
      form->length = st.st_size;
    }
  } else {
    form = NULL;
  }
  url = parsedURL2Str(&Currentbuf->content->url);
  scr_message("Reloading...", 0, 0);
  term_refresh();
  auto content = loadGeneralFile(url->ptr, NULL, NO_REFERER, true, form);

  if (multipart)
    unlink(form->body);
  if (!content) {
    message_push("Can't reload...");
    return;
  }
  if (fbuf != NULL)
    Firstbuf = deleteBuffer(Firstbuf, fbuf);
  // repBuffer(Currentbuf, buf);
  // if ((buf->type != NULL) && (sbuf.type != NULL) &&
  //     ((!strcasecmp(buf->type, "text/plain") && is_html_type(sbuf.type)) ||
  //      (is_html_type(buf->type) && !strcasecmp(sbuf.type, "text/plain"))))
  //      {
  //   vwSrc();
  //   if (Currentbuf != buf)
  //     Firstbuf = deleteBuffer(Firstbuf, buf);
  // }
  // Currentbuf->search_header = sbuf.search_header;
  // Currentbuf->form_submit = sbuf.form_submit;
  if (Currentbuf->document->firstLine) {
    COPY_BUFROOT(Currentbuf->document, &sbuf);
    restorePosition(Currentbuf->document, &sbuf);
  }
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document") {
  Currentbuf->document->need_reshape = true;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks") {
  chkURLBuffer(Currentbuf->document);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink") {
  int spos, epos;
  auto p = getCurWord(Currentbuf->document, &spos, &epos);
  if (!p)
    return;
  reAnchorWord(Currentbuf->document, Currentbuf->document->currentLine, spos,
               epos);
}

/* spawn external browser */
static void invoke_browser(char *url) {
  // Str cmd;
  // int bg = 0, len;
  //
  // const char *browser = NULL;
  // clearKeyData(); /* not allowed in w3m-control: */
  // browser = searchKeyData();
  // if (browser == NULL || *browser == '\0') {
  //   switch (prec_num) {
  //   case 0:
  //   case 1:
  //     browser = ExtBrowser;
  //     break;
  //   case 2:
  //     browser = ExtBrowser2;
  //     break;
  //   case 3:
  //     browser = ExtBrowser3;
  //     break;
  //   case 4:
  //     browser = ExtBrowser4;
  //     break;
  //   case 5:
  //     browser = ExtBrowser5;
  //     break;
  //   case 6:
  //     browser = ExtBrowser6;
  //     break;
  //   case 7:
  //     browser = ExtBrowser7;
  //     break;
  //   case 8:
  //     browser = ExtBrowser8;
  //     break;
  //   case 9:
  //     browser = ExtBrowser9;
  //     break;
  //   }
  //   if (browser == NULL || *browser == '\0') {
  //     browser = inputStr(Currentbuf->document, "Browse command: ", NULL);
  //   }
  // }
  // if (browser == NULL || *browser == '\0') {
  //   return;
  // }
  //
  // if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
  //     browser[len - 2] != '\\') {
  //   browser = allocStr(browser, len - 2);
  //   bg = 1;
  // }
  // cmd = myExtCommand(browser, shell_quote(url), false);
  // Strremovetrailingspaces(cmd);
  // term_fmTerm();
  // mySystem(cmd->ptr, bg);
  // term_fmInit();
}

DEFUN(extbrz, EXTERN, "Display using an external browser") {
  if (Currentbuf->document->bufferprop & BP_INTERNAL) {
    message_push("Can't browse...");
    return;
  }
  if (Currentbuf->content->url.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->content->url.file, "-")) {
    /* file is std input */
    message_push("Can't browse stdin");
    return;
  }
  invoke_browser(parsedURL2Str(&Currentbuf->content->url)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  auto a = retrieveCurrentAnchor(Currentbuf->document);
  if (a == NULL)
    return;
  auto pu = parseURL2(a->url, baseURL(Currentbuf));
  invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document") {
  struct Line *l = Currentbuf->document->currentLine;
  Str tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != NULL) {
    cur = l->real_linenumber;
    col = l->bwidth + Currentbuf->document->viewport.currentColumn +
          Currentbuf->document->viewport.cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    len = l->bwidth + l->width;
  }
  if (Currentbuf->document->lastLine)
    all = Currentbuf->document->lastLine->real_linenumber;
  tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5), col,
                len);

  message_push(tmp->ptr);
}

DEFUN(dispVer, VERSION, "Display the version of w3m") {
  message_push(Sprintf("w3m version %s", w3m_version)->ptr);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches") {
  if (WrapSearch) {
    WrapSearch = false;
    message_push("Wrap search off");
  } else {
    WrapSearch = true;
    message_push("Wrap search on");
  }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)") {
  auto content = execdict(inputStr(Currentbuf->document, "(dictionary)!", ""));
  pushContent(CurrentTab, content);
}

DEFUN(dictwordat, DICT_WORD_AT,
      "Execute dictionary command for word at cursor") {
  auto content = execdict(GetWord(Currentbuf->document));
  pushContent(CurrentTab, content);
}

void deleteFiles() {
  for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
    while (Firstbuf) {
      auto buf = Firstbuf->nextBuffer;
      discardBuffer(Firstbuf);
      Firstbuf = buf;
    }
  }

  tmpfile_deletefiles();
}

void w3m_exit(int i) {
  stopDownload();
  deleteFiles();
  free_ssl_ctx();
  disconnectFTP();
#ifdef HAVE_MKDTEMP
  auto tmp_dir = app_get_tmpdir();
  if (no_rc_dir && tmp_dir != rc_dir)
    if (rmdir(tmp_dir) != 0) {
      fprintf(stderr, "Can't remove temporary directory (%s)!\n", tmp_dir);
      exit(1);
    }
#endif
  exit(i);
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)") {
  clearKeyData(); /* not allowed in w3m-control: */
  auto data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist(nullptr, "command [; ...]: ", "", TextHist);
    if (data == NULL) {
      return;
    }
  }

  // /* data: FUNC [DATA] [; FUNC [DATA] ...] */
  // while (*data) {
  //   SKIP_BLANKS(data);
  //   if (*data == ';') {
  //     data++;
  //     continue;
  //   }
  //   auto p = getWord(&data);
  //   auto cmd = getFuncList(p);
  //   if (cmd < 0)
  //     break;
  //   p = getQWord(&data);
  //   CurrentKey = -1;
  //   CurrentKeyData = NULL;
  //   CurrentCmdData = *p ? p : NULL;
  //   w3mFuncList[cmd].func();
  //   CurrentCmdData = NULL;
  // }
}

DEFUN(setAlarm, ALARM, "Set alarm") {
  // assert(false);
}

DEFUN(reinit, REINIT, "Reload configuration file") {
  const char *resource = searchKeyData();
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
    initKeymap(true);
    return;
  }

  if (!strcasecmp(resource, "MIMETYPES")) {
    initMimeTypes();
    return;
  }

  message_push(Sprintf("Don't know how to reinitialize '%s'", resource)->ptr);
}

DEFUN(defKey, DEFINE_KEY,
      "Define a binding between a key stroke combination and a command") {
  clearKeyData(); /* not allowed in w3m-control: */
  const char *data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist(Currentbuf->document, "Key definition: ", "", TextHist);
    if (data == NULL || *data == '\0') {
      return;
    }
  }
  setKeymap(allocStr(data, -1), -1);
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)") {
  _newT(Currentbuf->content);
}

DEFUN(closeT, CLOSE_TAB, "Close tab") {
  if (nTab <= 1)
    return;

  deleteTab(CurrentTab);
}

DEFUN(nextT, NEXT_TAB, "Switch to the next tab") {
  if (nTab <= 1)
    return;
  for (int i = 0; i < precNum(); i++) {
    if (CurrentTab->nextTab)
      CurrentTab = CurrentTab->nextTab;
    else
      CurrentTab = FirstTab;
  }
}

DEFUN(prevT, PREV_TAB, "Switch to the previous tab") {
  if (nTab <= 1)
    return;
  for (int i = 0; i < precNum(); i++) {
    if (CurrentTab->prevTab)
      CurrentTab = CurrentTab->prevTab;
    else
      CurrentTab = LastTab;
  }
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab") {
  // followTab(prec_num ? numTab(PREC_NUM) : NULL);
  // auto a = retrieveCurrentAnchor(Currentbuf->document);
  // if (a == NULL)
  //   return;
  //
  // if (tab == CurrentTab) {
  //   check_target = false;
  //   auto buf = _followA();
  //   pushBuffer(tab, buf);
  //   check_target = true;
  //   return;
  // }
  //
  // auto buf = _followA();
  // _newT(buf);
}

// static void tabURL0(struct TabBuffer *tab, char *prompt, int relative) {
//   auto content = goURL0(prompt, relative);
//   pushDocument(tab, doc);
// }

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab") {
  // tabURL0(prec_num ? numTab(PREC_NUM) : NULL, "Goto URL on new tab: ",
  // false);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab") {
  // tabURL0(prec_num ? numTab(PREC_NUM) : NULL, "Goto relative URL on new
  // tab:
  // ", true);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar") {
  struct TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < precNum(); tab = tab->nextTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : LastTab, true);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar") {
  struct TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < precNum(); tab = tab->prevTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : FirstTab, false);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel") {
  // Buffer *buf;
  // int replace = false, new_tab = false;
  // int reload;
  //
  // if (Currentbuf->bufferprop & BP_INTERNAL &&
  //     !strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
  //   replace = true;
  // if (!FirstDL) {
  //   if (replace) {
  //     if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == NULL) {
  //       if (nTab > 1)
  //         deleteTab(CurrentTab);
  //     } else
  //       delBuffer(Currentbuf);
  //     displayBuffer(Currentbuf, B_FORCE_REDRAW);
  //   }
  //   return;
  // }
  // reload = checkDownloadList();
  // buf = DownloadListBuffer();
  // if (!buf) {
  //   displayBuffer(Currentbuf, B_NORMAL);
  //   return;
  // }
  // buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
  // if (replace) {
  //   COPY_BUFROOT(buf, Currentbuf);
  //   restorePosition(buf, Currentbuf);
  // }
  // if (!replace && open_tab_dl_list) {
  //   _newT();
  //   new_tab = true;
  // }
  // pushBuffer(CurrentTab, buf);
  // if (replace || new_tab)
  //   deletePrevBuf();
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement") {
  if (!Currentbuf->document->firstLine)
    return;
  struct BufferPos *b = Currentbuf->document->viewport.undo;
  if (!b || !b->prev)
    return;
  for (int i = 0; i < precNum() && b->prev; i++, b = b->prev)
    ;
  resetPos(Currentbuf->document, b);
}

DEFUN(redoPos, REDO, "Cancel the last undo") {
  if (!Currentbuf->document->firstLine)
    return;
  struct BufferPos *b = Currentbuf->document->viewport.undo;
  if (!b || !b->next)
    return;
  for (int i = 0; i < precNum() && b->next; i++, b = b->next)
    ;
  resetPos(Currentbuf->document, b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  Currentbuf->document->currentLine =
      lineSkip(&Currentbuf->document->viewport, Currentbuf->document->topLine,
               Currentbuf->document->lastLine, 0, false);
  arrangeLine(Currentbuf->document);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  int offsety = (Currentbuf->document->viewport.LINES - 1) / 2;
  Currentbuf->document->currentLine =
      currentLineSkip(Currentbuf->document->topLine, offsety, false);
  arrangeLine(Currentbuf->document);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen") {
  if (Currentbuf->document->firstLine == NULL)
    return;
  int offsety = Currentbuf->document->viewport.LINES - 1;
  Currentbuf->document->currentLine =
      currentLineSkip(Currentbuf->document->topLine, offsety, false);
  arrangeLine(Currentbuf->document);
}
