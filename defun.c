#define MAINPROGRAM
#include "fm.h"
#include "ftp.h"
#include "text.h"
#include "rc.h"
#include "search.h"
#include "trap_jmp.h"
#include "display.h"
#include "html_parser.h"
#include "loader.h"
#include "mailcap.h"
#include "alloc.h"
#include "http_response.h"
#include "cookie.h"
#include "file.h"
#include "indep.h"
#include "app.h"
#include "funcname1.h"
#include "history.h"
#include "parsetag.h"
#include "os.h"
#include "linein.h"
#include "downloadlist.h"
#include "http_request.h"
#include "localcgi.h"
#include "tabbuffer.h"
#include "buffer.h"
#include "map.h"
#include "scr.h"
#include "tty.h"
#include "defun.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "terms.h"
#include "termsize.h"
#include "myctype.h"
#include "regex.h"

#define HELP_CGI "w3mhelp"

#define DICTBUFFERNAME "*dictionary*"
#define DSTR_LEN 256

static void cmd_loadfile(const char *path);
static void cmd_loadURL(const char *url, struct Url *current,
                        const char *referer, struct FormList *request);
static void cmd_loadBuffer(struct Buffer *buf, int prop, int linkid);
static void keyPressEventProc(int c);

static char *getCurWord(struct Buffer *buf, int *spos, int *epos);

static int display_ok = false;
int prec_num = 0;
int prev_key = -1;
int on_target = 1;

void set_buffer_environ(struct Buffer *);
static void save_buffer_position(struct Document *buf);

struct TabBuffer;
static void _followForm(int);
static void _goLine(const char *);
static void followTab(struct TabBuffer *tab);
static void moveTab(struct TabBuffer *t, struct TabBuffer *t2, int right);
static void _nextA(int);
static void _prevA(int);
static int check_target = true;
#define PREC_NUM (prec_num ? prec_num : 1)
#define PREC_LIMIT 10000
static int searchKeyNum(void);

void mainloop(char *line_str) {

  if (line_str) {
    _goLine(line_str);
  }

  for (;;) {
    download_update();
    if (Currentbuf->submit) {
      struct Anchor *a = Currentbuf->submit;
      Currentbuf->submit = NULL;
      gotoLine(&Currentbuf->document, a->start.line);
      Currentbuf->document.pos = a->start.pos;
      _followForm(true);
      continue;
    }
    /* event processing */
    if (CurrentEvent) {
      CurrentKey = -1;
      CurrentKeyData = NULL;
      CurrentCmdData = (char *)CurrentEvent->data;
      w3mFuncList[CurrentEvent->cmd].func();
      CurrentCmdData = NULL;
      CurrentEvent = CurrentEvent->next;
      continue;
    }
#ifndef _WIN32
    /* get keypress event */
    mySignal(SIGWINCH, resize_hook);
#endif
    {
      // do {
      resize_screen_if_updated();
      // }
      // while (tty_sleep_till_anykey(1, 0) <= 0);
    }
    char c = tty_getch();
    if (c && IS_ASCII(c)) { /* Ascii */
      if (('0' <= c) && (c <= '9') &&
          (prec_num || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
        prec_num = prec_num * 10 + (int)(c - '0');
        if (prec_num > PREC_LIMIT)
          prec_num = PREC_LIMIT;
      } else {
        set_buffer_environ(Currentbuf);
        save_buffer_position(&Currentbuf->document);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
  }
}

static void keyPressEventProc(int c) {
  CurrentKey = c;
  w3mFuncList[(int)GlobalKeymap[c]].func();
}

static int cmp_anchor_hseq(const void *a, const void *b) {
  return (*((const struct Anchor **)a))->hseq -
         (*((const struct Anchor **)b))->hseq;
}

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing") { /* do nothing */ }

void pcmap(void) {}

static void escKeyProc(int c, int esc, unsigned char *map) {
  if (CurrentKey >= 0 && CurrentKey & K_MULTI) {
    unsigned char **mmap;
    mmap = (unsigned char **)getKeyData(MULTI_KEY(CurrentKey));
    if (!mmap)
      return;
    switch (esc) {
    case K_ESCD:
      map = mmap[3];
      break;
    case K_ESCB:
      map = mmap[2];
      break;
    case K_ESC:
      map = mmap[1];
      break;
    default:
      map = mmap[0];
      break;
    }
    esc |= (CurrentKey & ~0xFFFF);
  }
  CurrentKey = esc | c;
  w3mFuncList[(int)map[c]].func();
}

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

void escdmap(char c) {
  int d;
  d = (int)c - (int)'0';
  c = tty_getch();
  if (IS_DIGIT(c)) {
    d = d * 10 + (int)c - (int)'0';
    c = tty_getch();
  }
  if (c == '~')
    escKeyProc((int)d, K_ESCD, EscDKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap") {
  char c = tty_getch();
  if (IS_ASCII(c)) {
    CurrentKey = K_MULTI | (CurrentKey << 16) | c;
    escKeyProc((int)c, 0, NULL);
  }
}

static Str currentURL(void);

void saveBufferInfo() {
  FILE *fp;

  if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
    return;
  }
  fprintf(fp, "%s\n", currentURL()->ptr);
  fclose(fp);
}

static void pushBuffer(struct Buffer *buf) {
  if (clear_buffer)
    tmpClearBuffer(&Currentbuf->document);

  struct Buffer *b;
  if (Firstbuf == Currentbuf) {
    buf->nextBuffer = Firstbuf;
    Firstbuf = Currentbuf = buf;
  } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
    b->nextBuffer = buf;
    buf->nextBuffer = Currentbuf;
    Currentbuf = buf;
  }
  saveBufferInfo();
}

void delBuffer(struct Buffer *buf) {
  if (buf == NULL)
    return;
  if (Currentbuf == buf)
    Currentbuf = buf->nextBuffer;
  Firstbuf = deleteBuffer(Firstbuf, buf);
  if (!Currentbuf)
    Currentbuf = Firstbuf;
}

static void repBuffer(struct Buffer *oldbuf, struct Buffer *buf) {
  Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
  Currentbuf = buf;
}

/*
 * Command functions: These functions are called with a keystroke.
 */

static void nscroll(int n, int mode) {
  struct Buffer *buf = Currentbuf;
  struct Line *top = buf->document.topLine, *cur = buf->document.currentLine;
  int lnum, tlnum, llnum, diff_n;

  if (buf->document.firstLine == NULL)
    return;
  lnum = cur->linenumber;
  buf->document.topLine = lineSkip(&buf->document, top, n, false);
  if (buf->document.topLine == top) {
    lnum += n;
    if (lnum < buf->document.topLine->linenumber)
      lnum = buf->document.topLine->linenumber;
    else if (lnum > buf->document.lastLine->linenumber)
      lnum = buf->document.lastLine->linenumber;
  } else {
    tlnum = buf->document.topLine->linenumber;
    llnum = buf->document.topLine->linenumber + buf->document.LINES - 1;
    if (nextpage_topline)
      diff_n = 0;
    else
      diff_n = n - (tlnum - top->linenumber);
    if (lnum < tlnum)
      lnum = tlnum + diff_n;
    if (lnum > llnum)
      lnum = llnum + diff_n;
  }
  gotoLine(&buf->document, lnum);
  arrangeLine(&buf->document);
  if (n > 0) {
    if (buf->document.currentLine->bpos &&
        buf->document.currentLine->bwidth >=
            buf->document.currentColumn + buf->document.visualpos)
      cursorDown(&buf->document, 1);
    else {
      while (buf->document.currentLine->next &&
             buf->document.currentLine->next->bpos &&
             buf->document.currentLine->bwidth +
                     buf->document.currentLine->width <
                 buf->document.currentColumn + buf->document.visualpos)
        cursorDown0(&buf->document, 1);
    }
  } else {
    if (buf->document.currentLine->bwidth + buf->document.currentLine->width <
        buf->document.currentColumn + buf->document.visualpos)
      cursorUp(&buf->document, 1);
    else {
      while (buf->document.currentLine->prev &&
             buf->document.currentLine->bpos &&
             buf->document.currentLine->bwidth >=
                 buf->document.currentColumn + buf->document.visualpos)
        cursorUp0(&buf->document, 1);
    }
  }
  displayBuffer(buf, mode);
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page") {
  if (vi_prec_num)
    nscroll(searchKeyNum() * (Currentbuf->document.LINES - 1), B_NORMAL);
  else
    nscroll(prec_num ? searchKeyNum()
                     : searchKeyNum() * (Currentbuf->document.LINES - 1),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page") {
  if (vi_prec_num)
    nscroll(-searchKeyNum() * (Currentbuf->document.LINES - 1), B_NORMAL);
  else
    nscroll(-(prec_num ? searchKeyNum()
                       : searchKeyNum() * (Currentbuf->document.LINES - 1)),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page") {
  nscroll(searchKeyNum() * (Currentbuf->document.LINES / 2 - 1), B_NORMAL);
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page") {
  nscroll(-searchKeyNum() * (Currentbuf->document.LINES / 2 - 1), B_NORMAL);
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
  if (Currentbuf->document.firstLine == NULL)
    return;
  offsety = Currentbuf->document.LINES / 2 - Currentbuf->document.cursorY;
  if (offsety != 0) {
    Currentbuf->document.topLine = lineSkip(
        &Currentbuf->document, Currentbuf->document.topLine, -offsety, false);
    arrangeLine(&Currentbuf->document);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column") {
  int offsetx;
  if (Currentbuf->document.firstLine == NULL)
    return;
  offsetx = Currentbuf->document.cursorX - Currentbuf->document.COLS / 2;
  if (offsetx != 0) {
    columnSkip(&Currentbuf->document, offsetx);
    arrangeCursor(&Currentbuf->document);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew") {
  scr_clear();
  term_clear();
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Search regular expression forward */

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

static void shiftvisualpos(struct Buffer *buf, int shift) {
  struct Line *l = buf->document.currentLine;
  buf->document.visualpos -= shift;
  if (buf->document.visualpos - l->bwidth >= buf->document.COLS)
    buf->document.visualpos = l->bwidth + buf->document.COLS - 1;
  else if (buf->document.visualpos - l->bwidth < 0)
    buf->document.visualpos = l->bwidth;
  arrangeLine(&buf->document);
  if (buf->document.visualpos - l->bwidth == -shift &&
      buf->document.cursorX == 0)
    buf->document.visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left") {
  int column;

  if (Currentbuf->document.firstLine == NULL)
    return;
  column = Currentbuf->document.currentColumn;
  columnSkip(&Currentbuf->document,
             searchKeyNum() * (-Currentbuf->document.COLS + 1) + 1);
  shiftvisualpos(Currentbuf, Currentbuf->document.currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right") {
  int column;

  if (Currentbuf->document.firstLine == NULL)
    return;
  column = Currentbuf->document.currentColumn;
  columnSkip(&Currentbuf->document,
             searchKeyNum() * (Currentbuf->document.COLS - 1) - 1);
  shiftvisualpos(Currentbuf, Currentbuf->document.currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1R, RIGHT, "Shift screen one column right") {
  struct Buffer *buf = Currentbuf;
  struct Line *l = buf->document.currentLine;
  int j, column, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    column = buf->document.currentColumn;
    columnSkip(&Currentbuf->document, 1);
    if (column == buf->document.currentColumn)
      break;
    shiftvisualpos(Currentbuf, 1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1L, LEFT, "Shift screen one column left") {
  struct Buffer *buf = Currentbuf;
  struct Line *l = buf->document.currentLine;
  int j, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    if (buf->document.currentColumn == 0)
      break;
    columnSkip(&Currentbuf->document, -1);
    shiftvisualpos(Currentbuf, -1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setEnv, SETENV, "Set environment variable") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  const char *env = searchKeyData();
  if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
    if (env != NULL && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    env = inputStrHist("Set environ: ", env, TextHist);
    if (env == NULL || *env == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  char *value;
  if ((value = strchr(env, '=')) != NULL && value > env) {
    auto var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  const char *cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd == NULL || *cmd == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  // auto prevtrap = mySignal(SIGINT, intTrap);
  tty_crmode();
  auto buf = getshell(cmd);
  // mySignal(SIGINT, prevtrap);
  tty_raw();
  if (buf == NULL) {
    /* FIXME: gettextize? */
    disp_message("Execution failed", true);
    return;
  } else {
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (buf->type == NULL)
      buf->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  const char *cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd != NULL && *cmd != '\0') {
    term_fmTerm();
    printf("\n");
    system(cmd);
    /* FIXME: gettextize? */
    printf("\n[Hit any key]");
    fflush(stdout);
    term_fmInit();
    tty_getch();
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer") {
  const char *fn = searchKeyData();
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

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel") {
  char *lang = AcceptLang;
  int n = strcspn(lang, ";, \t");
  Str tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
}

static void cmd_loadfile(const char *fn) {
  struct Buffer *buf =
      loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor left */
static void _movL(int n) {
  int m = searchKeyNum();
  if (Currentbuf->document.firstLine == NULL)
    return;
  for (int i = 0; i < m; i++)
    cursorLeft(&Currentbuf->document, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movL, MOVE_LEFT, "Cursor left") { _movL(Currentbuf->document.COLS / 2); }

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide") { _movL(1); }

/* Move cursor downward */
static void _movD(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document.firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorDown(&Currentbuf->document, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movD, MOVE_DOWN, "Cursor down") {
  _movD((Currentbuf->document.LINES + 1) / 2);
}

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide") { _movD(1); }

/* move cursor upward */
static void _movU(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document.firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorUp(&Currentbuf->document, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movU, MOVE_UP, "Cursor up") {
  _movU((Currentbuf->document.LINES + 1) / 2);
}

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide") { _movU(1); }

/* Move cursor right */
static void _movR(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->document.firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorRight(&Currentbuf->document, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right") {
  _movR(Currentbuf->document.COLS / 2);
}

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide") {
  _movR(1);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */
#if defined(USE_M17N) && defined(USE_UNICODE)
#define nextChar(s, l)                                                         \
  do {                                                                         \
    (s)++;                                                                     \
  } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)
#define prevChar(s, l)                                                         \
  do {                                                                         \
    (s)--;                                                                     \
  } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)

static wc_uint32 getChar(char *p) {
  return wc_any_to_ucs(wtf_parse1((wc_uchar **)&p));
}

static int is_wordchar(wc_uint32 c) { return wc_is_ucs_alnum(c); }
#else
#define nextChar(s, l) (s)++
#define prevChar(s, l) (s)--
#define getChar(p) ((int)*(p))

static int is_wordchar(int c) { return IS_ALNUM(c); }
#endif

static int prev_nonnull_line(struct Line *line) {
  struct Line *l;

  for (l = line; l != NULL && l->len == 0; l = l->prev)
    ;
  if (l == NULL || l->len == 0)
    return -1;

  Currentbuf->document.currentLine = l;
  if (l != line)
    Currentbuf->document.pos = Currentbuf->document.currentLine->len;
  return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word") {
  char *lb;
  struct Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->document.firstLine == NULL)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->document.currentLine;
    ppos = Currentbuf->document.pos;

    if (prev_nonnull_line(Currentbuf->document.currentLine) < 0)
      goto end;

    while (1) {
      l = Currentbuf->document.currentLine;
      lb = l->lineBuf;
      while (Currentbuf->document.pos > 0) {
        int tmp = Currentbuf->document.pos;
        prevChar(tmp, l);
        if (is_wordchar(getChar(&lb[tmp])))
          break;
        Currentbuf->document.pos = tmp;
      }
      if (Currentbuf->document.pos > 0)
        break;
      if (prev_nonnull_line(Currentbuf->document.currentLine->prev) < 0) {
        Currentbuf->document.currentLine = pline;
        Currentbuf->document.pos = ppos;
        goto end;
      }
      Currentbuf->document.pos = Currentbuf->document.currentLine->len;
    }

    l = Currentbuf->document.currentLine;
    lb = l->lineBuf;
    while (Currentbuf->document.pos > 0) {
      int tmp = Currentbuf->document.pos;
      prevChar(tmp, l);
      if (!is_wordchar(getChar(&lb[tmp])))
        break;
      Currentbuf->document.pos = tmp;
    }
  }
end:
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

static int next_nonnull_line(struct Line *line) {
  struct Line *l;

  for (l = line; l != NULL && l->len == 0; l = l->next)
    ;

  if (l == NULL || l->len == 0)
    return -1;

  Currentbuf->document.currentLine = l;
  if (l != line)
    Currentbuf->document.pos = 0;
  return 0;
}

DEFUN(movRW, NEXT_WORD, "Move to the next word") {
  char *lb;
  struct Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->document.firstLine == NULL)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->document.currentLine;
    ppos = Currentbuf->document.pos;

    if (next_nonnull_line(Currentbuf->document.currentLine) < 0)
      goto end;

    l = Currentbuf->document.currentLine;
    lb = l->lineBuf;
    while (Currentbuf->document.pos < l->len &&
           is_wordchar(getChar(&lb[Currentbuf->document.pos])))
      nextChar(Currentbuf->document.pos, l);

    while (1) {
      while (Currentbuf->document.pos < l->len &&
             !is_wordchar(getChar(&lb[Currentbuf->document.pos])))
        nextChar(Currentbuf->document.pos, l);
      if (Currentbuf->document.pos < l->len)
        break;
      if (next_nonnull_line(Currentbuf->document.currentLine->next) < 0) {
        Currentbuf->document.currentLine = pline;
        Currentbuf->document.pos = ppos;
        goto end;
      }
      Currentbuf->document.pos = 0;
      l = Currentbuf->document.currentLine;
      lb = l->lineBuf;
    }
  }
end:
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

static void _quitfm(int confirm) {
  const char *ans = "y";

  if (checkDownloadList())
    /* FIXME: gettextize? */
    ans = inputChar("Download process retains. "
                    "Do you want to exit w3m? (y/n)");
  else if (confirm)
    /* FIXME: gettextize? */
    ans = inputChar("Do you want to exit w3m? (y/n)");
  if (!(ans && TOLOWER(*ans) == 'y')) {
    displayBuffer(Currentbuf, B_NORMAL);
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
  struct Buffer *buf;
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
      delBuffer(buf);
      if (Firstbuf == NULL) {
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

  for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
    if (buf == Currentbuf)
      continue;
    if (clear_buffer)
      tmpClearBuffer(&buf->document);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background") {
#ifndef SIGSTOP
  char *shell;
#endif /* not SIGSTOP */
  scr_move(LASTLINE, 0);
  scr_clrtoeolx();
  term_refresh();
  term_fmTerm();
#ifndef SIGSTOP
  shell = getenv("SHELL");
  if (shell == NULL)
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
  term_fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Go to specified line */
static void _goLine(const char *l) {
  if (l == NULL || *l == '\0' || Currentbuf->document.currentLine == NULL) {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  Currentbuf->document.pos = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    gotoRealLine(Currentbuf, prec_num);
  } else if (*l == '^') {
    Currentbuf->document.topLine = Currentbuf->document.currentLine =
        Currentbuf->document.firstLine;
  } else if (*l == '$') {
    Currentbuf->document.topLine =
        lineSkip(&Currentbuf->document, Currentbuf->document.lastLine,
                 -(Currentbuf->document.LINES + 1) / 2, true);
    Currentbuf->document.currentLine = Currentbuf->document.lastLine;
  } else
    gotoRealLine(Currentbuf, atoi(l));
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line") {
  const char *str = searchKeyData();
  if (prec_num)
    _goLine("^");
  else if (str)
    _goLine(str);
  else
    /* FIXME: gettextize? */
    _goLine(inputStr("Goto line: ", ""));
}

DEFUN(goLineF, BEGIN, "Go to the first line") { _goLine("^"); }

DEFUN(goLineL, END, "Go to the last line") { _goLine("$"); }

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line") {
  if (Currentbuf->document.firstLine == NULL)
    return;
  while (Currentbuf->document.currentLine->prev &&
         Currentbuf->document.currentLine->bpos)
    cursorUp0(&Currentbuf->document, 1);
  Currentbuf->document.pos = 0;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line") {
  if (Currentbuf->document.firstLine == NULL)
    return;
  while (Currentbuf->document.currentLine->next &&
         Currentbuf->document.currentLine->next->bpos)
    cursorDown0(&Currentbuf->document, 1);
  Currentbuf->document.pos = Currentbuf->document.currentLine->len - 1;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

static int cur_real_linenumber(struct Buffer *buf) {
  struct Line *l, *cur = buf->document.currentLine;
  int n;

  if (!cur)
    return 1;
  n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (l = buf->document.firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source") {
  const char *fn = Currentbuf->filename;
  Str cmd;

  if (fn == NULL ||
      (Currentbuf->type == NULL &&
       Currentbuf->edit == NULL) || /* Reading shell */
      Currentbuf->real_scheme != SCM_LOCAL ||
      !strcmp(Currentbuf->currentURL.file, "-") || /* file is std input  */
      Currentbuf->bufferprop & BP_FRAME) {         /* Frame */
    disp_err_message("Can't edit other than local file", true);
    return;
  }
  if (Currentbuf->edit)
    cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
                          checkHeader(Currentbuf, "Content-Type:"), NULL);
  else
    cmd = myEditor(Editor, shell_quote(fn), cur_real_linenumber(Currentbuf));
  term_fmTerm();
  system(cmd->ptr);
  term_fmInit();

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document") {
  auto tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
  auto f = fopen(tmpf, "w");
  if (f == NULL) {
    /* FIXME: gettextize? */
    disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, true);
    return;
  }
  saveBuffer(Currentbuf, f, true);
  fclose(f);
  term_fmTerm();
  system(myEditor(Editor, shell_quote(tmpf), cur_real_linenumber(Currentbuf))
             ->ptr);
  term_fmInit();
  unlink(tmpf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static struct Buffer *loadNormalBuf(struct Buffer *buf) {
  pushBuffer(buf);
  return buf;
}

static struct Buffer *loadLink(const char *url, const char *target,
                               const char *referer, struct FormList *request) {
  struct Buffer *buf, *nfbuf;
  union frameset_element *f_element = NULL;
  int flag = 0;
  struct Url *base, pu;
  const int *no_referer_ptr;

  scr_message(Sprintf("loading %s", url)->ptr, 0, 0);
  term_refresh();

  no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
  base = baseURL(Currentbuf);
  if ((no_referer_ptr && *no_referer_ptr) || base == NULL ||
      base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI ||
      base->scheme == SCM_DATA)
    referer = NO_REFERER;
  if (referer == NULL)
    referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
  buf = loadGeneralFile(url, baseURL(Currentbuf), referer, flag, request);
  if (buf == NULL) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
    return NULL;
  }

  parseURL2(url, &pu, base);
  pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

  if (buf == NO_BUFFER) {
    return NULL;
  }
  if (!on_target) /* open link as an indivisual page */
    return loadNormalBuf(buf);

  if (do_download) /* download (thus no need to render frames) */
    return loadNormalBuf(buf);

  if (target == NULL || /* no target specified (that means this page is not a
                           frame page) */
      !strcmp(target, "_top") || /* this link is specified to be opened as an
                                    indivisual * page */
      !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
  ) {
    return loadNormalBuf(buf);
  }
  /* original page (that contains <frameset> tag) doesn't exist */
  return loadNormalBuf(buf);
}

static void gotoLabel(const char *label) {
  auto al = searchURLLabel(&Currentbuf->document, label);
  if (al == NULL) {
    /* FIXME: gettextize? */
    disp_message(Sprintf("%s is not found", label)->ptr, true);
    return;
  }

  auto buf = newBuffer(Currentbuf->document.width);
  copyBuffer(&buf->document, &Currentbuf->document);
  for (int i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = NULL;
  buf->currentURL.label = allocStr(label, -1);
  pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
  (*buf->clone)++;
  pushBuffer(buf);
  gotoLine(&Currentbuf->document, al->start.line);
  if (label_topline)
    Currentbuf->document.topLine =
        lineSkip(&Currentbuf->document, Currentbuf->document.topLine,
                 Currentbuf->document.currentLine->linenumber -
                     Currentbuf->document.topLine->linenumber,
                 false);
  Currentbuf->document.pos = al->start.pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  return;
}

static int handleMailto(const char *url) {
  Str to;
  char *pos;

  if (strncasecmp(url, "mailto:", 7))
    return 0;
  if (!non_null(Mailer)) {
    /* FIXME: gettextize? */
    disp_err_message("no mailer is specified", true);
    return 1;
  }

  /* invoke external mailer */
  if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
    to = Strnew_charp(html_unquote(url));
  } else {
    to = Strnew_charp(url + 7);
    if ((pos = strchr(to->ptr, '?')) != NULL)
      Strtruncate(to, pos - to->ptr);
  }
  term_fmTerm();
  system(myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)), false)->ptr);
  term_fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  pushHashHist(URLHist, url);
  return 1;
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer") {
  struct Url u;

  if (Currentbuf->document.firstLine == NULL)
    return;

  auto a = retrieveCurrentMap(Currentbuf);
  if (a) {
    _followForm(false);
    return;
  }
  a = retrieveCurrentAnchor(&Currentbuf->document);
  if (a == NULL) {
    _followForm(false);
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
  auto url = a->url;

  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    struct Buffer *buf;

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

/* follow HREF link in the buffer */
void bufferA(void) {
  on_target = false;
  followA();
  on_target = true;
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer") {
  struct Anchor *a;
  struct Buffer *buf;

  if (Currentbuf->document.firstLine == NULL)
    return;

  a = retrieveCurrentImg(&Currentbuf->document);
  if (a == NULL)
    return;
  /* FIXME: gettextize? */
  scr_message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  term_refresh();
  buf = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

static struct FormItemList *save_submit_formlist(struct FormItemList *src) {
  struct FormList *list;
  struct FormList *srclist;
  struct FormItemList *srcitem;
  struct FormItemList *item;
  struct FormItemList *ret = NULL;

  if (src == NULL)
    return NULL;
  srclist = src->parent;
  list = New(struct FormList);
  list->method = srclist->method;
  list->action = Strdup(srclist->action);
  list->enctype = srclist->enctype;
  list->nitems = srclist->nitems;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
    item = New(struct FormItemList);
    item->type = srcitem->type;
    item->name = Strdup(srcitem->name);
    item->value = Strdup(srcitem->value);
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    item->next = NULL;

    if (list->lastitem == NULL) {
      list->item = list->lastitem = item;
    } else {
      list->lastitem->next = item;
      list->lastitem = item;
    }

    if (srcitem == src)
      ret = item;
  }

  return ret;
}

#define conv_form_encoding(val, fi, buf) (val)

static void query_from_followform(Str *query, struct FormItemList *fi,
                                  int multipart) {
  struct FormItemList *f2;
  FILE *body = NULL;

  if (multipart) {
    *query = tmpfname(TMPF_DFL, NULL);
    body = fopen((*query)->ptr, "w");
    if (body == NULL) {
      return;
    }
    fi->parent->body = (*query)->ptr;
    fi->parent->boundary =
        Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
                fi->parent, fi->parent->body, fi->parent->boundary)
            ->ptr;
  }
  *query = Strnew();
  for (f2 = fi->parent->item; f2; f2 = f2->next) {
    if (f2->name == NULL)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != fi || f2->value == NULL)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    if (multipart) {
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
        Strcat_charp(*query, ".x");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", x)->ptr);
        *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
        Strcat_charp(*query, ".y");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", y)->ptr);
      } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
        /* not IMAGE */
        *query = conv_form_encoding(f2->value, fi, Currentbuf);
        if (f2->type == FORM_INPUT_FILE)
          form_write_from_file(
              body, fi->parent->boundary,
              conv_form_encoding(f2->name, fi, Currentbuf)->ptr, (*query)->ptr,
              Str_conv_to_system(f2->value)->ptr);
        else
          form_write_data(body, fi->parent->boundary,
                          conv_form_encoding(f2->name, fi, Currentbuf)->ptr,
                          (*query)->ptr);
      }
    } else {
      /* not multipart */
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        Strcat(*query,
               Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
        Strcat(*query, Sprintf(".x=%d&", x));
        Strcat(*query,
               Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
        Strcat(*query, Sprintf(".y=%d", y));
      } else {
        /* not IMAGE */
        if (f2->name && f2->name->length > 0) {
          Strcat(*query,
                 Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
          Strcat_char(*query, '=');
        }
        if (f2->value != NULL) {
          if (fi->parent->method == FORM_METHOD_INTERNAL)
            Strcat(*query, Str_form_quote(f2->value));
          else {
            Strcat(*query, Str_form_quote(
                               conv_form_encoding(f2->value, fi, Currentbuf)));
          }
        }
      }
      if (f2->next)
        Strcat_char(*query, '&');
    }
  }
  if (multipart) {
    fprintf(body, "--%s--\r\n", fi->parent->boundary);
    fclose(body);
  } else {
    /* remove trailing & */
    while (Strlastchar(*query) == '&')
      Strshrink(*query, 1);
  }
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form") { _followForm(true); }

/* process form */
void followForm(void) { _followForm(false); }

static void _followForm(int submit) {
  struct Anchor *a, *a2;
  struct FormItemList *fi, *f2;
  Str tmp, tmp2;
  int multipart = 0, i;

  if (Currentbuf->document.firstLine == NULL)
    return;

  a = retrieveCurrentForm(&Currentbuf->document);
  if (a == NULL)
    return;
  fi = (struct FormItemList *)a->url;

  const char *p;
  switch (fi->type) {
  case FORM_INPUT_TEXT:
    if (submit)
      goto do_submit;
    if (fi->readonly)
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
    /* FIXME: gettextize? */
    p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
    if (p == NULL || fi->readonly)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(a, Currentbuf, fi);
    if (fi->accept || fi->parent->nitems == 1)
      goto do_submit;
    break;
  case FORM_INPUT_FILE:
    if (submit)
      goto do_submit;
    if (fi->readonly)
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
    /* FIXME: gettextize? */
    p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL, NULL);
    if (p == NULL || fi->readonly)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(a, Currentbuf, fi);
    if (fi->accept || fi->parent->nitems == 1)
      goto do_submit;
    break;
  case FORM_INPUT_PASSWORD:
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    /* FIXME: gettextize? */
    p = inputLine("Password:", fi->value ? fi->value->ptr : NULL, IN_PASSWORD);
    if (p == NULL)
      break;
    fi->value = Strnew_charp(p);
    formUpdateBuffer(a, Currentbuf, fi);
    if (fi->accept)
      goto do_submit;
    break;
  case FORM_TEXTAREA:
    if (submit)
      goto do_submit;
    if (fi->readonly)
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
    input_textarea(fi);
    formUpdateBuffer(a, Currentbuf, fi);
    break;
  case FORM_INPUT_RADIO:
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    formRecheckRadio(a, Currentbuf, fi);
    break;
  case FORM_INPUT_CHECKBOX:
    if (submit)
      goto do_submit;
    if (fi->readonly) {
      /* FIXME: gettextize? */
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(a, Currentbuf, fi);
    break;
  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
  do_submit:
    tmp = Strnew();
    multipart = (fi->parent->method == FORM_METHOD_POST &&
                 fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    query_from_followform(&tmp, fi, multipart);

    tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
      /* It means "current URL" */
      tmp2 = parsedURL2Str(&Currentbuf->currentURL);
      if ((p = strchr(tmp2->ptr, '?')) != NULL)
        Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
      if ((p = strchr(tmp2->ptr, '?')) != NULL)
        Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
      Strcat_charp(tmp2, "?");
      Strcat(tmp2, tmp);
      loadLink(tmp2->ptr, a->target, NULL, NULL);
    } else if (fi->parent->method == FORM_METHOD_POST) {
      struct Buffer *buf;
      if (multipart) {
        struct stat st;
        stat(fi->parent->body, &st);
        fi->parent->length = st.st_size;
      } else {
        fi->parent->body = tmp->ptr;
        fi->parent->length = tmp->length;
      }
      buf = loadLink(tmp2->ptr, a->target, NULL, fi->parent);
      if (multipart) {
        unlink(fi->parent->body);
      }
      if (buf &&
          !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
        /* BP_REDIRECTED means that the buffer is obtained through
         * Location: header. In this case, buf->form_submit must not be set
         * because the page is not loaded by POST method but GET method.
         */
        buf->form_submit = save_submit_formlist(fi);
      }
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
                (!Strcmp_charp(fi->parent->action, "map") ||
                 !Strcmp_charp(fi->parent->action, "none"))) ||
               Currentbuf->bufferprop & BP_INTERNAL) { /* internal */
      do_internal(tmp2->ptr, tmp->ptr);
    } else {
      disp_err_message("Can't send form because of illegal method.", false);
    }
    break;
  case FORM_INPUT_RESET:
    for (i = 0; i < Currentbuf->document.formitem->nanchor; i++) {
      a2 = &Currentbuf->document.formitem->anchors[i];
      f2 = (struct FormItemList *)a2->url;
      if (f2->parent == fi->parent && f2->name && f2->value &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        formUpdateBuffer(a2, Currentbuf, f2);
      }
    }
    break;
  case FORM_INPUT_HIDDEN:
  default:
    break;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink") {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct BufferPoint *po;
  struct Anchor *an;
  int hseq = 0;

  if (Currentbuf->document.firstLine == NULL)
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
    an = retrieveAnchor(Currentbuf->document.href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->document.formitem, po->line, po->pos);
    hseq++;
  } while (an == NULL);

  gotoLine(&Currentbuf->document, po->line);
  Currentbuf->document.pos = po->pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink") {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct BufferPoint *po;
  struct Anchor *an;
  int hseq;

  if (Currentbuf->document.firstLine == NULL)
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
    an = retrieveAnchor(Currentbuf->document.href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->document.formitem, po->line, po->pos);
    hseq--;
  } while (an == NULL);

  gotoLine(&Currentbuf->document, po->line);
  Currentbuf->document.pos = po->pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link") {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct BufferPoint *po;
  struct Anchor *an;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  po = hl->marks + n - 1;
  an = retrieveAnchor(Currentbuf->document.href, po->line, po->pos);
  if (an == NULL)
    an = retrieveAnchor(Currentbuf->document.formitem, po->line, po->pos);
  if (an == NULL)
    return;

  gotoLine(&Currentbuf->document, po->line);
  Currentbuf->document.pos = po->pos;
  arrangeCursor(&Currentbuf->document);
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

/* go to the next [visited] anchor */
static void _nextA(int visited) {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct BufferPoint *po;
  struct Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  struct Url url;

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(&Currentbuf->document);
  if (visited != true && an == NULL)
    an = retrieveCurrentForm(&Currentbuf->document);

  y = Currentbuf->document.currentLine->linenumber;
  x = Currentbuf->document.pos;

  if (visited == true) {
    n = hl->nmark;
  }

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq + 1;
      do {
        if (hseq >= hl->nmark) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        po = &hl->marks[hseq];
        an = retrieveAnchor(Currentbuf->document.href, po->line, po->pos);
        if (visited != true && an == NULL)
          an = retrieveAnchor(Currentbuf->document.formitem, po->line, po->pos);
        hseq++;
        if (visited == true && an) {
          parseURL2(an->url, &url, baseURL(Currentbuf));
          if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
            goto _end;
          }
        }
      } while (an == NULL || an == pan);
    } else {
      an = closest_next_anchor(Currentbuf->document.href, NULL, x, y);
      if (visited != true)
        an = closest_next_anchor(Currentbuf->document.formitem, an, x, y);
      if (an == NULL) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true) {
        parseURL2(an->url, &url, baseURL(Currentbuf));
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
  po = &hl->marks[an->hseq];
  gotoLine(&Currentbuf->document, po->line);
  Currentbuf->document.pos = po->pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous anchor */
static void _prevA(int visited) {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct BufferPoint *po;
  struct Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  struct Url url;

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(&Currentbuf->document);
  if (visited != true && an == NULL)
    an = retrieveCurrentForm(&Currentbuf->document);

  y = Currentbuf->document.currentLine->linenumber;
  x = Currentbuf->document.pos;

  if (visited == true) {
    n = hl->nmark;
  }

  for (i = 0; i < n; i++) {
    pan = an;
    if (an && an->hseq >= 0) {
      int hseq = an->hseq - 1;
      do {
        if (hseq < 0) {
          if (visited == true)
            return;
          an = pan;
          goto _end;
        }
        po = hl->marks + hseq;
        an = retrieveAnchor(Currentbuf->document.href, po->line, po->pos);
        if (visited != true && an == NULL)
          an = retrieveAnchor(Currentbuf->document.formitem, po->line, po->pos);
        hseq--;
        if (visited == true && an) {
          parseURL2(an->url, &url, baseURL(Currentbuf));
          if (getHashHist(URLHist, parsedURL2Str(&url)->ptr)) {
            goto _end;
          }
        }
      } while (an == NULL || an == pan);
    } else {
      an = closest_prev_anchor(Currentbuf->document.href, NULL, x, y);
      if (visited != true)
        an = closest_prev_anchor(Currentbuf->document.formitem, an, x, y);
      if (an == NULL) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true && an) {
        parseURL2(an->url, &url, baseURL(Currentbuf));
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
  po = hl->marks + an->hseq;
  gotoLine(&Currentbuf->document, po->line);
  Currentbuf->document.pos = po->pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left/right anchor */
static void nextX(int d, int dy) {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct Anchor *an, *pan;
  struct Line *l;
  int i, x, y, n = searchKeyNum();

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(&Currentbuf->document);
  if (an == NULL)
    an = retrieveCurrentForm(&Currentbuf->document);

  l = Currentbuf->document.currentLine;
  x = Currentbuf->document.pos;
  y = l->linenumber;
  pan = NULL;
  for (i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = NULL;
    while (1) {
      for (; x >= 0 && x < l->len; x += d) {
        an = retrieveAnchor(Currentbuf->document.href, y, x);
        if (!an)
          an = retrieveAnchor(Currentbuf->document.formitem, y, x);
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
  gotoLine(&Currentbuf->document, y);
  Currentbuf->document.pos = pan->start.pos;
  arrangeCursor(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next downward/upward anchor */
static void nextY(int d) {
  struct HmarkerList *hl = Currentbuf->document.hmarklist;
  struct Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  int hseq;

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(&Currentbuf->document);
  if (an == NULL)
    an = retrieveCurrentForm(&Currentbuf->document);

  x = Currentbuf->document.pos;
  y = Currentbuf->document.currentLine->linenumber + d;
  pan = NULL;
  hseq = -1;
  for (i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = NULL;
    for (; y >= 0 && y <= Currentbuf->document.lastLine->linenumber; y += d) {
      an = retrieveAnchor(Currentbuf->document.href, y, x);
      if (!an)
        an = retrieveAnchor(Currentbuf->document.formitem, y, x);
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
  gotoLine(&Currentbuf->document, pan->start.line);
  arrangeLine(&Currentbuf->document);
  displayBuffer(Currentbuf, B_NORMAL);
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
  struct Buffer *buf;
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
  struct Buffer *buf;
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

static int checkBackBuffer(struct Buffer *buf) {
  if (buf->nextBuffer)
    return true;

  return false;
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK,
      "Close current buffer and return to the one below in stack") {
  if (!checkBackBuffer(Currentbuf)) {
    if (close_tab_back && nTab >= 1) {
      deleteTab(CurrentTab);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
    } else
      /* FIXME: gettextize? */
      disp_message("Can't go back...", true);
    return;
  }

  delBuffer(Currentbuf);

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local CGI-scripts)") {
  struct Buffer *buf = Currentbuf->nextBuffer;
  if (buf)
    delBuffer(buf);
}

static void cmd_loadURL(const char *url, struct Url *current,
                        const char *referer, struct FormList *request) {
  struct Buffer *buf;

  if (handleMailto(url))
    return;

  term_refresh();
  buf = loadGeneralFile(url, current, referer, 0, request);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
static void goURL0(char *prompt, int relative) {
  const char *url, *referer;
  struct Url p_url, *current;
  struct Buffer *cur_buf = Currentbuf;
  const int *no_referer_ptr;

  url = searchKeyData();
  if (url == NULL) {
    struct Hist *hist = copyHist(URLHist);
    struct Anchor *a;

    current = baseURL(Currentbuf);
    if (current) {
      char *c_url = parsedURL2Str(current)->ptr;
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url);
      else
        pushHist(hist, c_url);
    }
    a = retrieveCurrentAnchor(&Currentbuf->document);
    if (a) {
      char *a_url;
      parseURL2(a->url, &p_url, current);
      a_url = parsedURL2Str(&p_url)->ptr;
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url);
      else
        pushHist(hist, a_url);
    }
    url = inputLineHist(prompt, url, IN_URL, hist);
    if (url != NULL)
      SKIP_BLANKS(url);
  }
  if (relative) {
    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
    current = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || current == NULL ||
        current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI ||
        current->scheme == SCM_DATA)
      referer = NO_REFERER;
    else
      referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
    url = url_quote(url);
  } else {
    current = NULL;
    referer = NULL;
    url = url_quote(url);
  }
  if (url == NULL || *url == '\0') {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  if (*url == '#') {
    gotoLabel(url + 1);
    return;
  }
  parseURL2(url, &p_url, current);
  pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
  cmd_loadURL(url, current, referer, NULL);
  if (Currentbuf != cur_buf) /* success */
    pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer") {
  goURL0("Goto URL: ", false);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer") {
  const char *url;
  if ((url = getenv("HTTP_HOME")) != NULL ||
      (url = getenv("WWW_HOME")) != NULL) {
    struct Url p_url;
    struct Buffer *cur_buf = Currentbuf;
    SKIP_BLANKS(url);
    url = url_quote(url);
    parseURL2(url, &p_url, NULL);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, NULL, NULL, NULL);
    if (Currentbuf != cur_buf) /* success */
      pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
  }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address") {
  goURL0("Goto relative URL: ", true);
}

static void cmd_loadBuffer(struct Buffer *buf, int prop, int linkid) {
  if (buf == NULL) {
    disp_err_message("Can't load string", false);
  } else if (buf != NO_BUFFER) {
    buf->bufferprop |= (BP_INTERNAL | prop);
    if (!(buf->bufferprop & BP_NO_URL))
      copyParsedURL(&buf->currentURL, &Currentbuf->currentURL);
    if (linkid != LB_NOLINK) {
      buf->linkBuffer[REV_LB[linkid]] = Currentbuf;
      Currentbuf->linkBuffer[linkid] = buf;
    }
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks") {
  cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks") {
  Str tmp;
  struct FormList *request;

  tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
                (Str_form_quote(localCookie()))->ptr,
                (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
                (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->ptr,
                (Str_form_quote(Strnew_charp(Currentbuf->buffername)))->ptr);
  request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
  request->body = tmp->ptr;
  request->length = tmp->length;
  cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER, request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel") {
  cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  const char *opt = searchKeyData();
  if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
    if (opt != NULL && *opt != '\0') {
      auto v = get_param_option(opt);
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
DEFUN(msgs, MSGS, "Display error messages") {
  cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document") {
  struct Buffer *buf;

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

void follow_map(struct parsed_tagarg *arg) {
  char *name = tag_get_value(arg, "link");
#if defined(MENU_MAP) || defined(USE_IMAGE)
  struct Anchor *an;
  struct MapArea *a;
  int x, y;
  struct Url p_url;

  an = retrieveCurrentImg(&Currentbuf->document);
  x = Currentbuf->document.cursorX + Currentbuf->document.rootX;
  y = Currentbuf->document.cursorY + Currentbuf->document.rootY;
  a = follow_map_menu(Currentbuf, name, an, x, y);
  if (a == NULL || a->url == NULL || *(a->url) == '\0') {
#endif
    struct Buffer *buf = follow_map_panel(Currentbuf, name);

    if (buf != NULL)
      cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#if defined(MENU_MAP) || defined(USE_IMAGE)
    return;
  }
  if (*(a->url) == '#') {
    gotoLabel(a->url + 1);
    return;
  }
  parseURL2(a->url, &p_url, baseURL(Currentbuf));
  pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    struct Buffer *buf;

    _newT();
    buf = Currentbuf;
    cmd_loadURL(a->url, baseURL(Currentbuf),
                parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  cmd_loadURL(a->url, baseURL(Currentbuf),
              parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
#endif
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced") {
  struct Buffer *buf;

  buf = link_list_panel(Currentbuf);
  if (buf != NULL) {
    cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
  }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list") {
  struct Buffer *buf;

  buf = cookie_list_panel();
  if (buf != NULL)
    cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history") {
  cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  do_download = true;
  followA();
  do_download = false;
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  do_download = true;
  followI();
  do_download = false;
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document") {
  const char *qfile = NULL, *file;
  FILE *f;
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
    is_pipe = true;
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
    is_pipe = false;
  }
  if (f == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
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
  char *file;

  if (Currentbuf->sourcefile == NULL)
    return;
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;
  if (Currentbuf->real_scheme == SCM_LOCAL)
    file = conv_from_system(
        guess_save_name(NULL, Currentbuf->currentURL.real_file));
  else
    file = guess_save_name(Currentbuf, Currentbuf->currentURL.file);
  doFileCopy(Currentbuf->sourcefile, file);
  PermitSaveToPipe = false;
  displayBuffer(Currentbuf, B_NORMAL);
}

static void _peekURL(int only_img) {

  struct Anchor *a;
  struct Url pu;
  static Str s = NULL;
  static int offset = 0, n;

  if (Currentbuf->document.firstLine == NULL)
    return;
  if (CurrentKey == prev_key && s != NULL) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = NULL;
  a = (only_img ? NULL : retrieveCurrentAnchor(&Currentbuf->document));
  if (a == NULL) {
    a = (only_img ? NULL : retrieveCurrentForm(&Currentbuf->document));
    if (a == NULL) {
      a = retrieveCurrentImg(&Currentbuf->document);
      if (a == NULL)
        return;
    } else
      s = Strnew_charp(form2str((struct FormItemList *)a->url));
  }
  if (s == NULL) {
    parseURL2(a->url, &pu, baseURL(Currentbuf));
    s = parsedURL2Str(&pu);
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message_nomouse(&s->ptr[offset], true);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address") { _peekURL(0); }

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address") { _peekURL(1); }

/* show current URL */
static Str currentURL(void) {
  if (Currentbuf->bufferprop & BP_INTERNAL)
    return Strnew_size(0);
  return parsedURL2Str(&Currentbuf->currentURL);
}

DEFUN(curURL, PEEK, "Show current address") {
  static Str s = NULL;
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
      s = Strnew_charp(url_decode0(s->ptr));
  }
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message_nomouse(&s->ptr[offset], true);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed") {
  struct Buffer *buf;

  if (Currentbuf->type == NULL || Currentbuf->bufferprop & BP_FRAME)
    return;
  if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL ||
      (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (Currentbuf->sourcefile == NULL) {
    return;
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
    if (Currentbuf->real_type &&
        !strcasecmp(Currentbuf->real_type, "text/plain"))
      buf->real_type = "text/html";
    else
      buf->real_type = Currentbuf->real_type;
    buf->buffername = Sprintf("HTML view of %s", Currentbuf->buffername)->ptr;
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
  buf->clone = Currentbuf->clone;
  (*buf->clone)++;

  buf->need_reshape = true;
  reshapeBuffer(buf);
  pushBuffer(buf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew") {
  struct Buffer *buf, *fbuf = NULL;
  Str url;
  struct FormList *request;
  int multipart;

  if (Currentbuf->bufferprop & BP_INTERNAL) {
    if (!strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE)) {
      ldDL();
      return;
    }
    /* FIXME: gettextize? */
    disp_err_message("Can't reload...", true);
    return;
  }
  if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->currentURL.file, "-")) {
    /* file is std input */
    /* FIXME: gettextize? */
    disp_err_message("Can't reload stdin", true);
    return;
  }
  struct Document sbuf;
  copyBuffer(&sbuf, &Currentbuf->document);
  multipart = 0;
  if (Currentbuf->form_submit) {
    request = Currentbuf->form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
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
  scr_message("Reloading...", 0, 0);
  term_refresh();
  SearchHeader = Currentbuf->search_header;
  DefaultType = Currentbuf->real_type;
  buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
  SearchHeader = false;
  DefaultType = NULL;

  if (multipart)
    unlink(request->body);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't reload...", true);
    return;
  } else if (buf == NO_BUFFER) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (fbuf != NULL)
    Firstbuf = deleteBuffer(Firstbuf, fbuf);
  repBuffer(Currentbuf, buf);
  // if ((buf->type != NULL) && (sbuf.type != NULL) &&
  //     ((!strcasecmp(buf->type, "text/plain") && is_html_type(sbuf.type)) ||
  //      (is_html_type(buf->type) && !strcasecmp(sbuf.type, "text/plain")))) {
  //   vwSrc();
  //   if (Currentbuf != buf)
  //     Firstbuf = deleteBuffer(Firstbuf, buf);
  // }
  // Currentbuf->search_header = sbuf.search_header;
  // Currentbuf->form_submit = sbuf.form_submit;
  if (Currentbuf->document.firstLine) {
    COPY_BUFROOT(&Currentbuf->document, &sbuf);
    restorePosition(&Currentbuf->document, &sbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document") {
  Currentbuf->need_reshape = true;
  reshapeBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(struct Buffer *buf) {
  static char *url_like_pat[] = {
      "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/"
      "=\\-]",
      "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
      "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
      "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
      "https?://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
      "?=~_\\&+@#,\\$;]*",
      "ftp://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
      NULL};
  int i;
  for (i = 0; url_like_pat[i]; i++) {
    reAnchor(buf, url_like_pat[i]);
  }
  buf->check_url |= CHK_URL;
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks") {
  chkURLBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink") {
  char *p;
  int spos, epos;
  p = getCurWord(Currentbuf, &spos, &epos);
  if (p == NULL)
    return;
  reAnchorWord(Currentbuf, Currentbuf->document.currentLine, spos, epos);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* spawn external browser */
static void invoke_browser(char *url) {
  Str cmd;
  int bg = 0, len;

  const char *browser = NULL;
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  browser = searchKeyData();
  if (browser == NULL || *browser == '\0') {
    switch (prec_num) {
    case 0:
    case 1:
      browser = ExtBrowser;
      break;
    case 2:
      browser = ExtBrowser2;
      break;
    case 3:
      browser = ExtBrowser3;
      break;
    case 4:
      browser = ExtBrowser4;
      break;
    case 5:
      browser = ExtBrowser5;
      break;
    case 6:
      browser = ExtBrowser6;
      break;
    case 7:
      browser = ExtBrowser7;
      break;
    case 8:
      browser = ExtBrowser8;
      break;
    case 9:
      browser = ExtBrowser9;
      break;
    }
    if (browser == NULL || *browser == '\0') {
      browser = inputStr("Browse command: ", NULL);
      if (browser != NULL)
        browser = conv_to_system(browser);
    }
  } else {
    browser = conv_to_system(browser);
  }
  if (browser == NULL || *browser == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }

  if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
      browser[len - 2] != '\\') {
    browser = allocStr(browser, len - 2);
    bg = 1;
  }
  cmd = myExtCommand(browser, shell_quote(url), false);
  Strremovetrailingspaces(cmd);
  term_fmTerm();
  mySystem(cmd->ptr, bg);
  term_fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(extbrz, EXTERN, "Display using an external browser") {
  if (Currentbuf->bufferprop & BP_INTERNAL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't browse...", true);
    return;
  }
  if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->currentURL.file, "-")) {
    /* file is std input */
    /* FIXME: gettextize? */
    disp_err_message("Can't browse stdin", true);
    return;
  }
  invoke_browser(parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser") {
  struct Anchor *a;
  struct Url pu;

  if (Currentbuf->document.firstLine == NULL)
    return;
  a = retrieveCurrentAnchor(&Currentbuf->document);
  if (a == NULL)
    return;
  parseURL2(a->url, &pu, baseURL(Currentbuf));
  invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document") {
  struct Line *l = Currentbuf->document.currentLine;
  Str tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != NULL) {
    cur = l->real_linenumber;
    col = l->bwidth + Currentbuf->document.currentColumn +
          Currentbuf->document.cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    len = l->bwidth + l->width;
  }
  if (Currentbuf->document.lastLine)
    all = Currentbuf->document.lastLine->real_linenumber;
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
    /* FIXME: gettextize? */
    disp_message("Wrap search off", true);
  } else {
    WrapSearch = true;
    /* FIXME: gettextize? */
    disp_message("Wrap search on", true);
  }
}

static char *getCurWord(struct Buffer *buf, int *spos, int *epos) {
  char *p;
  struct Line *l = buf->document.currentLine;
  int b, e;

  *spos = 0;
  *epos = 0;
  if (l == NULL)
    return NULL;
  p = l->lineBuf;
  e = buf->document.pos;
  while (e > 0 && !is_wordchar(getChar(&p[e])))
    prevChar(e, l);
  if (!is_wordchar(getChar(&p[e])))
    return NULL;
  b = e;
  while (b > 0) {
    int tmp = b;
    prevChar(tmp, l);
    if (!is_wordchar(getChar(&p[tmp])))
      break;
    b = tmp;
  }
  while (e < l->len && is_wordchar(getChar(&p[e])))
    nextChar(e, l);
  *spos = b;
  *epos = e;
  return &p[b];
}

static char *GetWord(struct Buffer *buf) {
  int b, e;
  char *p;

  if ((p = getCurWord(buf, &b, &e)) != NULL) {
    return Strnew_charp_n(p, e - b)->ptr;
  }
  return NULL;
}

static void execdict(const char *word) {
  if (!UseDictCommand || word == NULL || *word == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  const char *w = conv_to_system(word);
  if (*w == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  auto dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  auto buf = loadGeneralFile(dictcmd, NULL, NO_REFERER, 0, NULL);
  if (buf == NULL) {
    disp_message("Execution failed", true);
    return;
  } else if (buf != NO_BUFFER) {
    buf->filename = w;
    buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->type == NULL)
      buf->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)") {
  execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
      "Execute dictionary command for word at cursor") {
  execdict(GetWord(Currentbuf));
}

void set_buffer_environ(struct Buffer *buf) {
  static struct Buffer *prev_buf = NULL;
  static struct Line *prev_line = NULL;
  static int prev_pos = -1;

  if (buf == NULL)
    return;

  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->sourcefile);
    set_environ("W3M_FILENAME", buf->filename);
    set_environ("W3M_TITLE", buf->buffername);
    set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
    set_environ("W3M_TYPE", buf->real_type ? buf->real_type : "unknown");
  }
  auto l = buf->document.currentLine;
  if (l &&
      (buf != prev_buf || l != prev_line || buf->document.pos != prev_pos)) {
    struct Anchor *a;
    struct Url pu;
    char *s = GetWord(buf);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    a = retrieveCurrentAnchor(&buf->document);
    if (a) {
      parseURL2(a->url, &pu, baseURL(buf));
      set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(&buf->document);
    if (a) {
      parseURL2(a->url, &pu, baseURL(buf));
      set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = retrieveCurrentForm(&buf->document);
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((struct FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ(
        "W3M_CURRENT_COLUMN",
        Sprintf("%d", buf->document.currentColumn + buf->document.cursorX + 1)
            ->ptr);
  } else if (!l) {
    set_environ("W3M_CURRENT_WORD", "");
    set_environ("W3M_CURRENT_LINK", "");
    set_environ("W3M_CURRENT_IMG", "");
    set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", "0");
    set_environ("W3M_CURRENT_COLUMN", "0");
  }
  prev_buf = buf;
  prev_line = l;
  prev_pos = buf->document.pos;
}

const char *searchKeyData(void) {
  const char *data = NULL;

  if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
    data = CurrentKeyData;
  else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
    data = CurrentCmdData;
  else if (CurrentKey >= 0)
    data = getKeyData(CurrentKey);
  CurrentKeyData = NULL;
  CurrentCmdData = NULL;
  if (data == NULL || *data == '\0')
    return NULL;
  return allocStr(data, -1);
}

static int searchKeyNum(void) {
  int n = 1;
  const char *d = searchKeyData();
  if (d != NULL)
    n = atoi(d);
  return n * PREC_NUM;
}

void deleteFiles() {
  struct Buffer *buf;
  char *f;

  for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
    while (Firstbuf && Firstbuf != NO_BUFFER) {
      buf = Firstbuf->nextBuffer;
      discardBuffer(Firstbuf);
      Firstbuf = buf;
    }
  }
  while ((f = popText(fileToDelete)) != NULL) {
    unlink(f);
    if (enable_inline_image == INLINE_IMG_SIXEL &&
        strcmp(f + strlen(f) - 4, ".gif") == 0) {
      Str firstframe = Strnew_charp(f);
      Strcat_charp(firstframe, "-1");
      unlink(firstframe->ptr);
    }
  }
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
  const char *data, *p;
  int cmd;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  data = searchKeyData();
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
    p = getWord(&data);
    cmd = getFuncList(p);
    if (cmd < 0)
      break;
    p = getQWord(&data);
    CurrentKey = -1;
    CurrentKeyData = NULL;
    CurrentCmdData = *p ? p : NULL;
    w3mFuncList[cmd].func();
    CurrentCmdData = NULL;
  }
  displayBuffer(Currentbuf, B_NORMAL);
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
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  const char *data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist("Key definition: ", "", TextHist);
    if (data == NULL || *data == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  setKeymap(allocStr(data, -1), -1, true);
  displayBuffer(Currentbuf, B_NORMAL);
}

struct TabBuffer *newTab(void) {
  struct TabBuffer *n;

  n = New(struct TabBuffer);
  if (n == NULL)
    return NULL;
  n->nextTab = NULL;
  n->currentBuffer = NULL;
  n->firstBuffer = NULL;
  return n;
}

void _newT(void) {
  auto tag = newTab();
  if (!tag)
    return;

  auto buf = newBuffer(Currentbuf->document.width);
  copyBuffer(&buf->document, &Currentbuf->document);
  buf->nextBuffer = NULL;
  for (int i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = NULL;
  (*buf->clone)++;
  tag->firstBuffer = tag->currentBuffer = buf;

  tag->nextTab = CurrentTab->nextTab;
  tag->prevTab = CurrentTab;
  if (CurrentTab->nextTab)
    CurrentTab->nextTab->prevTab = tag;
  else
    LastTab = tag;
  CurrentTab->nextTab = tag;
  CurrentTab = tag;
  nTab++;
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)") {
  _newT();
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

static struct TabBuffer *numTab(int n) {
  struct TabBuffer *tab;
  int i;

  if (n == 0)
    return CurrentTab;
  if (n == 1)
    return FirstTab;
  if (nTab <= 1)
    return NULL;
  for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void calcTabPos(void) {
  struct TabBuffer *tab;
  int lcol = 0, rcol = 0, col;
  int n1, n2, na, nx, ny, ix, iy;

  if (nTab <= 0)
    return;
  n1 = (COLS - rcol - lcol) / TabCols;
  if (n1 >= nTab) {
    n2 = 1;
    ny = 1;
  } else {
    if (n1 < 0)
      n1 = 0;
    n2 = COLS / TabCols;
    if (n2 == 0)
      n2 = 1;
    ny = (nTab - n1 - 1) / n2 + 2;
  }
  na = n1 + n2 * (ny - 1);
  n1 -= (na - nTab) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);
  tab = FirstTab;
  for (iy = 0; iy < ny && tab; iy++) {
    if (iy == 0) {
      nx = n1;
      col = COLS - rcol - lcol;
    } else {
      nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
      col = COLS;
    }
    for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
      tab->x1 = col * ix / nx;
      tab->x2 = col * (ix + 1) / nx - 1;
      tab->y = iy;
      if (iy == 0) {
        tab->x1 += lcol;
        tab->x2 += lcol;
      }
    }
  }
}

struct TabBuffer *deleteTab(struct TabBuffer *tab) {
  struct Buffer *buf, *next;

  if (nTab <= 1)
    return FirstTab;
  if (tab->prevTab) {
    if (tab->nextTab)
      tab->nextTab->prevTab = tab->prevTab;
    else
      LastTab = tab->prevTab;
    tab->prevTab->nextTab = tab->nextTab;
    if (tab == CurrentTab)
      CurrentTab = tab->prevTab;
  } else { /* tab == FirstTab */
    tab->nextTab->prevTab = NULL;
    FirstTab = tab->nextTab;
    if (tab == CurrentTab)
      CurrentTab = tab->nextTab;
  }
  nTab--;
  buf = tab->firstBuffer;
  while (buf && buf != NO_BUFFER) {
    next = buf->nextBuffer;
    discardBuffer(buf);
    buf = next;
  }
  return FirstTab;
}

DEFUN(closeT, CLOSE_TAB, "Close tab") {
  struct TabBuffer *tab;

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

static void followTab(struct TabBuffer *tab) {
  struct Buffer *buf;
  struct Anchor *a;

  a = retrieveCurrentAnchor(&Currentbuf->document);
  if (a == NULL)
    return;

  if (tab == CurrentTab) {
    check_target = false;
    followA();
    check_target = true;
    return;
  }
  _newT();
  buf = Currentbuf;
  check_target = false;
  followA();
  check_target = true;
  if (tab == NULL) {
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    struct Buffer *c, *p;

    c = Currentbuf;
    p = prevBuffer(c, buf);
    p->nextBuffer = NULL;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = prevBuffer(c, buf);
      pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab") {
  followTab(prec_num ? numTab(PREC_NUM) : NULL);
}

static void tabURL0(struct TabBuffer *tab, char *prompt, int relative) {
  struct Buffer *buf;

  if (tab == CurrentTab) {
    goURL0(prompt, relative);
    return;
  }
  _newT();
  buf = Currentbuf;
  goURL0(prompt, relative);
  if (tab == NULL) {
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    struct Buffer *c, *p;

    c = Currentbuf;
    p = prevBuffer(c, buf);
    p->nextBuffer = NULL;
    Firstbuf = buf;
    deleteTab(CurrentTab);
    CurrentTab = tab;
    for (buf = p; buf; buf = p) {
      p = prevBuffer(c, buf);
      pushBuffer(buf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : NULL, "Goto URL on new tab: ", false);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
          "Goto relative URL on new tab: ", true);
}

static void moveTab(struct TabBuffer *t, struct TabBuffer *t2, int right) {
  if (t2 == NO_TABBUFFER)
    t2 = FirstTab;
  if (!t || !t2 || t == t2 || t == NO_TABBUFFER)
    return;
  if (t->prevTab) {
    if (t->nextTab)
      t->nextTab->prevTab = t->prevTab;
    else
      LastTab = t->prevTab;
    t->prevTab->nextTab = t->nextTab;
  } else {
    t->nextTab->prevTab = NULL;
    FirstTab = t->nextTab;
  }
  if (right) {
    t->nextTab = t2->nextTab;
    t->prevTab = t2;
    if (t2->nextTab)
      t2->nextTab->prevTab = t;
    else
      LastTab = t;
    t2->nextTab = t;
  } else {
    t->prevTab = t2->prevTab;
    t->nextTab = t2;
    if (t2->prevTab)
      t2->prevTab->nextTab = t;
    else
      FirstTab = t;
    t2->prevTab = t;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar") {
  struct TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : LastTab, true);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar") {
  struct TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
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
  // pushBuffer(buf);
  // if (replace || new_tab)
  //   deletePrevBuf();
  // displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void save_buffer_position(struct Document *doc) {
  struct BufferPos *b = doc->undo;

  if (!doc->firstLine)
    return;
  if (b && b->top_linenumber == TOP_LINENUMBER(doc) &&
      b->cur_linenumber == CUR_LINENUMBER(doc) &&
      b->currentColumn == doc->currentColumn && b->pos == doc->pos)
    return;
  b = New(struct BufferPos);
  b->top_linenumber = TOP_LINENUMBER(doc);
  b->cur_linenumber = CUR_LINENUMBER(doc);
  b->currentColumn = doc->currentColumn;
  b->pos = doc->pos;
  b->bpos = doc->currentLine ? doc->currentLine->bpos : 0;
  b->next = NULL;
  b->prev = doc->undo;
  if (doc->undo)
    doc->undo->next = b;
  doc->undo = b;
}

static void resetPos(struct BufferPos *b) {
  struct Line top;
  top.linenumber = b->top_linenumber;
  struct Line cur;
  cur.linenumber = b->cur_linenumber;
  cur.bpos = b->bpos;
  struct Document doc;
  doc.topLine = &top;
  doc.currentLine = &cur;
  doc.pos = b->pos;
  doc.currentColumn = b->currentColumn;
  restorePosition(&Currentbuf->document, &doc);
  Currentbuf->document.undo = b;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement") {
  if (!Currentbuf->document.firstLine)
    return;
  struct BufferPos *b = Currentbuf->document.undo;
  if (!b || !b->prev)
    return;
  for (int i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
    ;
  resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo") {
  if (!Currentbuf->document.firstLine)
    return;
  struct BufferPos *b = Currentbuf->document.undo;
  if (!b || !b->next)
    return;
  for (int i = 0; i < PREC_NUM && b->next; i++, b = b->next)
    ;
  resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen") {
  if (Currentbuf->document.firstLine == NULL)
    return;
  Currentbuf->document.currentLine =
      lineSkip(&Currentbuf->document, Currentbuf->document.topLine, 0, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen") {
  int offsety;
  if (Currentbuf->document.firstLine == NULL)
    return;
  offsety = (Currentbuf->document.LINES - 1) / 2;
  Currentbuf->document.currentLine =
      currentLineSkip(Currentbuf->document.topLine, offsety, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen") {
  int offsety;
  if (Currentbuf->document.firstLine == NULL)
    return;
  offsety = Currentbuf->document.LINES - 1;
  Currentbuf->document.currentLine =
      currentLineSkip(Currentbuf->document.topLine, offsety, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}
