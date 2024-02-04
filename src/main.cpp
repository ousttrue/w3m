#include "alloc.h"
#include "quote.h"
#include "contentinfo.h"
#include "loadproc.h"
#include "bufferpos.h"
#include "app.h"
#include "etc.h"
#include "mimetypes.h"
#include "url_schema.h"
#include "url_stream.h"
#include "ssl_util.h"
#include "message.h"
#include "screen.h"
#include "tmpfile.h"
#include "readbuffer.h"
#include "alarm.h"
#include "w3m.h"
#include "httprequest.h"
#include "linein.h"
#include "search.h"
#include "proto.h"
#include "buffer.h"
#include "cookie.h"
#include "downloadlist.h"
#include "funcname1.h"
#include "istream.h"
#include "func.h"
#include "keyvalue.h"
#include "form.h"
#include "history.h"
#include "anchor.h"
#include "mailcap.h"
#include "local_cgi.h"
#include "signal_util.h"
#include "display.h"
#include "terms.h"
#include "myctype.h"
#include "regex.h"
#include "rc.h"
#include "util.h"
#include "proto.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <gc.h>

#define HELP_CGI "w3mhelp"
#define W3MBOOKMARK_CMDNAME "w3mbookmark"

// #define HOST_NAME_MAX 255
#define MAXIMUM_COLS 1024
#define DICTBUFFERNAME "*dictionary*"

#define DSTR_LEN 256

#define DEFUN(funcname, macroname, docstring) void funcname(void)

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

#define HAVE_GETCWD 1
char *currentdir() {
  char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
  path = (char *)NewAtom_N(char, MAXPATHLEN);
  getcwd(path, MAXPATHLEN);
#else
  path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
  path = NewAtom_N(char, 1024);
  getwd(path);
#else  /* not HAVE_GETWD */
  FILE *f;
  char *p;
  path = (char *)NewAtom_N(char, 1024);
  f = popen("pwd", "r");
  fgets(path, 1024, f);
  pclose(f);
  for (p = path; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
  return path;
}

Str *Str_form_quote(Str *x) {
  Str *tmp = NULL;
  char *p = x->ptr, *ep = x->ptr + x->length;
  char buf[4];

  for (; p < ep; p++) {
    if (*p == ' ') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, '+');
    } else if (is_url_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      sprintf(buf, "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return x;
}

static const char *SearchString = nullptr;
int (*searchRoutine)(Buffer *, const char *);

JMP_BUF IntReturn;

static void delBuffer(Buffer *buf);
static void cmd_loadfile(const char *path);
static void cmd_loadURL(const char *url, Url *current, const char *referer,
                        FormList *request);
static void cmd_loadBuffer(Buffer *buf, int prop, int linkid);
int show_params_p = 0;
void show_params(FILE *fp);

static void _goLine(const char *);
static void _newT(void);
static void followTab(TabBuffer *tab);
static void moveTab(TabBuffer *t, TabBuffer *t2, int right);
static void _nextA(int);
static void _prevA(int);
static int check_target = true;
static int searchKeyNum(void);

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void fversion(FILE *f) {
  fprintf(f, "w3m version %s, options %s\n", w3m_version,
#if LANG == JA
          "lang=ja"
#else
          "lang=en"
#endif
          ",cookie"
          ",ssl"
          ",ssl-verify"
#ifdef INET6
          ",ipv6"
#endif
          ",alarm");
}

static void fusage(FILE *f, int err) {
  fversion(f);
  fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
  fprintf(f, "    -t tab           set tab width\n");
  fprintf(f, "    -r               ignore backspace effect\n");
  fprintf(f, "    -l line          # of preserved line (default 10000)\n");
  fprintf(f, "    -B               load bookmark\n");
  fprintf(f, "    -bookmark file   specify bookmark file\n");
  fprintf(f, "    -T type          specify content-type\n");
  fprintf(f, "    -m               internet message mode\n");
  fprintf(f, "    -v               visual startup mode\n");
  fprintf(f, "    -N               open URL of command line on each new tab\n");
  fprintf(f, "    -F               automatically render frames\n");
  fprintf(f, "    -cols width      specify column width (used with -dump)\n");
  fprintf(f, "    -ppc count       specify the number of pixels per character "
             "(4.0...32.0)\n");
  fprintf(f, "    -dump            dump formatted page into stdout\n");
  fprintf(f,
          "    -dump_head       dump response of HEAD request into stdout\n");
  fprintf(f, "    -dump_source     dump page source into stdout\n");
  fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
  fprintf(f, "    -dump_extra      dump HEAD, source, and extra information "
             "into stdout\n");
  fprintf(f, "    -post file       use POST method with file content\n");
  fprintf(f, "    -header string   insert string as a header\n");
  fprintf(f, "    +<num>           goto <num> line\n");
  fprintf(f, "    -num             show line number\n");
  fprintf(f, "    -no-proxy        don't use proxy\n");
#ifdef INET6
  fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
  fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
#endif
  fprintf(f, "    -insecure        use insecure SSL config options\n");
  fprintf(f,
          "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
  fprintf(f, "    -graph           use DEC special graphics for border of "
             "table and menu\n");
  fprintf(f, "    -no-graph        use ASCII character for border of table and "
             "menu\n");
#if 1 /* pager requires -s */
  fprintf(f, "    -s               squeeze multiple blank lines\n");
#else
  fprintf(f, "    -S               squeeze multiple blank lines\n");
#endif
  fprintf(f, "    -W               toggle search wrap mode\n");
  fprintf(f, "    -X               don't use termcap init/deinit\n");
  fprintf(f, "    -title[=TERM]    set buffer name to terminal title string\n");
  fprintf(f, "    -o opt=value     assign value to config option\n");
  fprintf(f, "    -show-option     print all config options\n");
  fprintf(f, "    -config file     specify config file\n");
  fprintf(f, "    -debug           use debug mode (only for debugging)\n");
  fprintf(f, "    -reqlog          write request logfile\n");
  fprintf(f, "    -help            print this usage message\n");
  fprintf(f, "    -version         print w3m version\n");
  if (show_params_p)
    show_params(f);
  exit(err);
}

static GC_warn_proc orig_GC_warn_proc = nullptr;
#define GC_WARN_KEEP_MAX (20)

static void wrap_GC_warn_proc(char *msg, GC_word arg) {
  if (fmInitialized) {
    /* *INDENT-OFF* */
    static struct {
      char *msg;
      GC_word arg;
    } msg_ring[GC_WARN_KEEP_MAX];
    /* *INDENT-ON* */
    static int i = 0;
    static size_t n = 0;
    static int lock = 0;
    int j;

    j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
    msg_ring[j].msg = msg;
    msg_ring[j].arg = arg;

    if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
      ++n;
    else
      ++i;

    if (!lock) {
      lock = 1;

      for (; n > 0; --n, ++i) {
        i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

        printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
        sleep_till_anykey(1, 1);
      }

      lock = 0;
    }
  } else if (orig_GC_warn_proc)
    orig_GC_warn_proc(msg, arg);
  else
    fprintf(stderr, msg, (unsigned long)arg);
}

static void sig_chld(int signo) {
  int p_stat;
  pid_t pid;

  while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
    DownloadList *d;

    if (WIFEXITED(p_stat)) {
      for (d = FirstDL; d != nullptr; d = d->next) {
        if (d->pid == pid) {
          d->err = WEXITSTATUS(p_stat);
          break;
        }
      }
    }
  }
  mySignal(SIGCHLD, sig_chld);
  return;
}

static Str *make_optional_header_string(char *s) {
  char *p;
  Str *hs;

  if (strchr(s, '\n') || strchr(s, '\r'))
    return nullptr;
  for (p = s; *p && *p != ':'; p++)
    ;
  if (*p != ':' || p == s)
    return nullptr;
  hs = Strnew_size(strlen(s) + 3);
  Strcopy_charp_n(hs, s, p - s);
  if (!Strcasecmp_charp(hs, "content-type"))
    override_content_type = true;
  if (!Strcasecmp_charp(hs, "user-agent"))
    override_user_agent = true;
  Strcat_charp(hs, ": ");
  if (*(++p)) {     /* not null header */
    SKIP_BLANKS(p); /* skip white spaces */
    Strcat_charp(hs, p);
  }
  Strcat_charp(hs, "\r\n");
  return hs;
}

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
  /*
   * Suppress compiler warning: function might return no value
   * This code is never reached.
   */
  return nullptr;
}

static void intTrap(SIGNAL_ARG) { /* Interrupt catcher */
  LONGJMP(IntReturn, 0);
  SIGNAL_RETURN;
}

DEFUN(nulcmd, NOTHING nullptr @ @ @, "Do nothing") { /* do nothing */
}

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
  if (map)
    w3mFuncList[(int)map[c]].func();
}

DEFUN(escmap, ESCMAP, "ESC map") {
  char c;
  c = getch();
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map") {
  char c;
  c = getch();
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
  c = getch();
  if (IS_DIGIT(c)) {
    d = d * 10 + (int)c - (int)'0';
    c = getch();
  }
  if (c == '~')
    escKeyProc((int)d, K_ESCD, EscDKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap") {
  char c;
  c = getch();
  if (IS_ASCII(c)) {
    CurrentKey = K_MULTI | (CurrentKey << 16) | c;
    escKeyProc((int)c, 0, nullptr);
  }
}

static Str *currentURL(void);

void saveBufferInfo() {
  FILE *fp;
  if ((fp = fopen(rcFile("bufinfo"), "w")) == nullptr) {
    return;
  }
  fprintf(fp, "%s\n", currentURL()->ptr);
  fclose(fp);
}

static void pushBuffer(Buffer *buf) {
  Buffer *b;

  if (Firstbuf == Currentbuf) {
    buf->nextBuffer = Firstbuf;
    Firstbuf = Currentbuf = buf;
  } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != nullptr) {
    b->nextBuffer = buf;
    buf->nextBuffer = Currentbuf;
    Currentbuf = buf;
  }
  saveBufferInfo();
}

static void delBuffer(Buffer *buf) {
  if (buf == nullptr)
    return;
  if (Currentbuf == buf)
    Currentbuf = buf->nextBuffer;
  Firstbuf = deleteBuffer(Firstbuf, buf);
  if (!Currentbuf)
    Currentbuf = Firstbuf;
}

static void repBuffer(Buffer *oldbuf, Buffer *buf) {
  Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
  Currentbuf = buf;
}

static void SigPipe(SIGNAL_ARG) {
  mySignal(SIGPIPE, SigPipe);
  SIGNAL_RETURN;
}

/*
 * Command functions: These functions are called with a keystroke.
 */

static void nscroll(int n, DisplayFlag mode) {
  Buffer *buf = Currentbuf;
  Line *top = buf->topLine, *cur = buf->currentLine;
  int lnum, tlnum, llnum, diff_n;

  if (buf->firstLine == nullptr)
    return;
  lnum = cur->linenumber;
  buf->topLine = lineSkip(buf, top, n, false);
  if (buf->topLine == top) {
    lnum += n;
    if (lnum < buf->topLine->linenumber)
      lnum = buf->topLine->linenumber;
    else if (lnum > buf->lastLine->linenumber)
      lnum = buf->lastLine->linenumber;
  } else {
    tlnum = buf->topLine->linenumber;
    llnum = buf->topLine->linenumber + buf->LINES - 1;
    if (nextpage_topline)
      diff_n = 0;
    else
      diff_n = n - (tlnum - top->linenumber);
    if (lnum < tlnum)
      lnum = tlnum + diff_n;
    if (lnum > llnum)
      lnum = llnum + diff_n;
  }
  gotoLine(buf, lnum);
  arrangeLine(buf);
  if (n > 0) {
    if (buf->currentLine->bpos &&
        buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
      cursorDown(buf, 1);
    else {
      while (buf->currentLine->next && buf->currentLine->next->bpos &&
             buf->currentLine->bwidth + buf->currentLine->width() <
                 buf->currentColumn + buf->visualpos)
        cursorDown0(buf, 1);
    }
  } else {
    if (buf->currentLine->bwidth + buf->currentLine->width() <
        buf->currentColumn + buf->visualpos)
      cursorUp(buf, 1);
    else {
      while (buf->currentLine->prev && buf->currentLine->bpos &&
             buf->currentLine->bwidth >= buf->currentColumn + buf->visualpos)
        cursorUp0(buf, 1);
    }
  }
  displayBuffer(buf, mode);
}

/* Move page forward */
DEFUN(pgFore, NEXT_PAGE, "Scroll down one page") {
  if (vi_prec_num)
    nscroll(searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
  else
    nscroll(prec_num ? searchKeyNum()
                     : searchKeyNum() * (Currentbuf->LINES - 1),
            prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page") {
  if (vi_prec_num)
    nscroll(-searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
  else
    nscroll(
        -(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->LINES - 1)),
        prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page") {
  nscroll(searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page") {
  nscroll(-searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
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
  if (Currentbuf->firstLine == nullptr)
    return;
  offsety = Currentbuf->LINES / 2 - Currentbuf->cursorY;
  if (offsety != 0) {
    Currentbuf->topLine =
        lineSkip(Currentbuf, Currentbuf->topLine, -offsety, false);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column") {
  int offsetx;
  if (Currentbuf->firstLine == nullptr)
    return;
  offsetx = Currentbuf->cursorX - Currentbuf->COLS / 2;
  if (offsetx != 0) {
    columnSkip(Currentbuf, offsetx);
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew") {
  clear();
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void clear_mark(Line *l) {
  int pos;
  if (!l)
    return;
  for (pos = 0; pos < l->size(); pos++)
    l->propBuf[pos] &= ~PE_MARK;
}

/* search by regular expression */
static int srchcore(const char *str, int (*func)(Buffer *, const char *)) {
  MySignalHandler prevtrap = {};
  int i, result = SR_NOTFOUND;

  if (str != nullptr && str != SearchString)
    SearchString = str;
  if (SearchString == nullptr || *SearchString == '\0')
    return SR_NOTFOUND;

  str = SearchString;
  prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  if (SETJMP(IntReturn) == 0) {
    for (i = 0; i < PREC_NUM; i++) {
      result = func(Currentbuf, str);
      if (i < PREC_NUM - 1 && result & SR_FOUND)
        clear_mark(Currentbuf->currentLine);
    }
  }
  mySignal(SIGINT, prevtrap);
  term_raw();
  return result;
}

static void disp_srchresult(int result, const char *prompt, const char *str) {
  if (str == nullptr)
    str = "";
  if (result & SR_NOTFOUND)
    disp_message(Sprintf("Not found: %s", str)->ptr, true);
  else if (result & SR_WRAPPED)
    disp_message(Sprintf("Search wrapped: %s", str)->ptr, true);
  else if (show_srch_str)
    disp_message(Sprintf("%s%s", prompt, str)->ptr, true);
}

static int dispincsrch(int ch, Str *buf, Lineprop *prop) {
  bool do_next_search = false;

  static Buffer *sbuf = new Buffer(0);
  if (ch == 0 && buf == nullptr) {
    SAVE_BUFPOSITION(sbuf); /* search starting point */
    return -1;
  }

  auto str = buf->ptr;
  switch (ch) {
  case 022: /* C-r */
    searchRoutine = backwardSearch;
    do_next_search = true;
    break;
  case 023: /* C-s */
    searchRoutine = forwardSearch;
    do_next_search = true;
    break;

  default:
    if (ch >= 0)
      return ch; /* use InputKeymap */
  }

  if (do_next_search) {
    if (*str) {
      if (searchRoutine == forwardSearch)
        Currentbuf->pos += 1;
      SAVE_BUFPOSITION(sbuf);
      if (srchcore(str, searchRoutine) == SR_NOTFOUND &&
          searchRoutine == forwardSearch) {
        Currentbuf->pos -= 1;
        SAVE_BUFPOSITION(sbuf);
      }
      arrangeCursor(Currentbuf);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
      clear_mark(Currentbuf->currentLine);
      return -1;
    } else
      return 020; /* _prev completion for C-s C-s */
  } else if (*str) {
    RESTORE_BUFPOSITION(sbuf);
    arrangeCursor(Currentbuf);
    srchcore(str, searchRoutine);
    arrangeCursor(Currentbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  clear_mark(Currentbuf->currentLine);
  return -1;
}

static void isrch(int (*func)(Buffer *, const char *), const char *prompt) {
  auto sbuf = new Buffer(0);
  SAVE_BUFPOSITION(sbuf);
  dispincsrch(0, nullptr, nullptr); /* initialize incremental search state */

  searchRoutine = func;
  // auto str =
  //     inputLineHistSearch(prompt, nullptr, IN_STRING, TextHist, dispincsrch);
  // if (str == nullptr) {
  //   RESTORE_BUFPOSITION(&sbuf);
  // }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void srch(int (*func)(Buffer *, const char *), const char *prompt) {
  const char *str;
  int result;
  int disp = false;
  int pos;

  str = searchKeyData();
  if (str == nullptr || *str == '\0') {
    // str = inputStrHist(prompt, nullptr, TextHist);
    if (str != nullptr && *str == '\0')
      str = SearchString;
    if (str == nullptr) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    disp = true;
  }
  pos = Currentbuf->pos;
  if (func == forwardSearch)
    Currentbuf->pos += 1;
  result = srchcore(str, func);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->currentLine);
  else
    Currentbuf->pos = pos;
  displayBuffer(Currentbuf, B_NORMAL);
  if (disp)
    disp_srchresult(result, prompt, str);
  searchRoutine = func;
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

static void srch_nxtprv(int reverse) {
  int result;
  /* *INDENT-OFF* */
  static int (*routine[2])(Buffer *, const char *) = {forwardSearch,
                                                      backwardSearch};
  /* *INDENT-ON* */

  if (searchRoutine == nullptr) {
    disp_message("No previous regular expression", true);
    return;
  }
  if (reverse != 0)
    reverse = 1;
  if (searchRoutine == backwardSearch)
    reverse ^= 1;
  if (reverse == 0)
    Currentbuf->pos += 1;
  result = srchcore(SearchString, routine[reverse]);
  if (result & SR_FOUND)
    clear_mark(Currentbuf->currentLine);
  else {
    if (reverse == 0)
      Currentbuf->pos -= 1;
  }
  displayBuffer(Currentbuf, B_NORMAL);
  disp_srchresult(result, (reverse ? "Backward: " : "Forward: "), SearchString);
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward") { srch_nxtprv(0); }

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward") { srch_nxtprv(1); }

static void shiftvisualpos(Buffer *buf, int shift) {
  Line *l = buf->currentLine;
  buf->visualpos -= shift;
  if (buf->visualpos - l->bwidth >= buf->COLS)
    buf->visualpos = l->bwidth + buf->COLS - 1;
  else if (buf->visualpos - l->bwidth < 0)
    buf->visualpos = l->bwidth;
  arrangeLine(buf);
  if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
    buf->visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left") {
  int column;

  if (Currentbuf->firstLine == nullptr)
    return;
  column = Currentbuf->currentColumn;
  columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->COLS + 1) + 1);
  shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right") {
  int column;

  if (Currentbuf->firstLine == nullptr)
    return;
  column = Currentbuf->currentColumn;
  columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->COLS - 1) - 1);
  shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1R, RIGHT, "Shift screen one column right") {
  Buffer *buf = Currentbuf;
  Line *l = buf->currentLine;
  int j, column, n = searchKeyNum();

  if (l == nullptr)
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

DEFUN(col1L, LEFT, "Shift screen one column left") {
  Buffer *buf = Currentbuf;
  Line *l = buf->currentLine;
  int j, n = searchKeyNum();

  if (l == nullptr)
    return;
  for (j = 0; j < n; j++) {
    if (buf->currentColumn == 0)
      break;
    columnSkip(Currentbuf, -1);
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
  MySignalHandler prevtrap = {};
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
  prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  buf = getshell(cmd);
  mySignal(SIGINT, prevtrap);
  term_raw();
  if (buf == nullptr) {
    disp_message("Execution failed", true);
    return;
  } else {
    buf->bufferprop = (BufferFlags)(buf->bufferprop | BP_INTERNAL | BP_NO_URL);
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

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel") {
  auto lang = AcceptLang;
  auto n = strcspn(lang, ";, \t");
  auto tmp =
      Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
              Str_form_quote(Strnew_charp(w3m_version))->ptr,
              Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  cmd_loadURL(tmp->ptr, nullptr, NO_REFERER, nullptr);
}

static void cmd_loadfile(const char *fn) {

  auto buf =
      loadGeneralFile(file_to_url((char *)fn), {}, {.referer = NO_REFERER});
  if (buf == nullptr) {
    char *emsg = Sprintf("%s not found", fn)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor left */
static void _movL(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorLeft(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movL, MOVE_LEFT, "Cursor left") { _movL(Currentbuf->COLS / 2); }

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide") { _movL(1); }

/* Move cursor downward */
static void _movD(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorDown(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movD, MOVE_DOWN, "Cursor down") { _movD((Currentbuf->LINES + 1) / 2); }

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide") { _movD(1); }

/* move cursor upward */
static void _movU(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorUp(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movU, MOVE_UP, "Cursor up") { _movU((Currentbuf->LINES + 1) / 2); }

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide") { _movU(1); }

/* Move cursor right */
static void _movR(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == nullptr)
    return;
  for (i = 0; i < m; i++)
    cursorRight(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movR, MOVE_RIGHT, "Cursor right") { _movR(Currentbuf->COLS / 2); }

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide") {
  _movR(1);
}

/* movLW, movRW */
/*
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900
 */

static int prev_nonnull_line(Line *line) {
  Line *l;

  for (l = line; l != nullptr && l->len == 0; l = l->prev)
    ;
  if (l == nullptr || l->len == 0)
    return -1;

  Currentbuf->currentLine = l;
  if (l != line)
    Currentbuf->pos = Currentbuf->currentLine->len;
  return 0;
}

DEFUN(movLW, PREV_WORD, "Move to the previous word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->currentLine;
    ppos = Currentbuf->pos;

    if (prev_nonnull_line(Currentbuf->currentLine) < 0)
      goto end;

    while (1) {
      l = Currentbuf->currentLine;
      lb = l->lineBuf.data();
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
    lb = l->lineBuf.data();
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

static int next_nonnull_line(Line *line) {
  Line *l;

  for (l = line; l != nullptr && l->len == 0; l = l->next)
    ;

  if (l == nullptr || l->len == 0)
    return -1;

  Currentbuf->currentLine = l;
  if (l != line)
    Currentbuf->pos = 0;
  return 0;
}

DEFUN(movRW, NEXT_WORD, "Move to the next word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->firstLine == nullptr)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->currentLine;
    ppos = Currentbuf->pos;

    if (next_nonnull_line(Currentbuf->currentLine) < 0)
      goto end;

    l = Currentbuf->currentLine;
    lb = l->lineBuf.data();
    while (Currentbuf->pos < l->len &&
           is_wordchar(getChar(&lb[Currentbuf->pos])))
      nextChar(Currentbuf->pos, l);

    while (1) {
      while (Currentbuf->pos < l->len &&
             !is_wordchar(getChar(&lb[Currentbuf->pos])))
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
      lb = l->lineBuf.data();
    }
  }
end:
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

static void _quitfm(bool confirm) {
  const char *ans = "y";
  // if (checkDownloadList()) {
  //   ans = inputChar("Download process retains. "
  //                   "Do you want to exit w3m? (y/n)");
  // } else if (confirm) {
  //   ans = inputChar("Do you want to exit w3m? (y/n)");
  // }

  if (!(ans && TOLOWER(*ans) == 'y')) {
    // cancel
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }

  // exit
  term_title(""); /* XXX */
  fmTerm();
  save_cookies();
  if (UseHistory && SaveURLHist)
    saveHistory(URLHist, URLHistSize);
  w3m_exit(0);
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
      delBuffer(buf);
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

/* Go to specified line */
static void _goLine(const char *l) {
  if (l == nullptr || *l == '\0' || Currentbuf->currentLine == nullptr) {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  Currentbuf->pos = 0;
  if (((*l == '^') || (*l == '$')) && prec_num) {
    gotoRealLine(Currentbuf, prec_num);
  } else if (*l == '^') {
    Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
  } else if (*l == '$') {
    Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->lastLine,
                                   -(Currentbuf->LINES + 1) / 2, true);
    Currentbuf->currentLine = Currentbuf->lastLine;
  } else
    gotoRealLine(Currentbuf, atoi(l));
  arrangeCursor(Currentbuf);
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
  if (Currentbuf->firstLine == nullptr)
    return;
  while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
    cursorUp0(Currentbuf, 1);
  Currentbuf->pos = 0;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line") {
  if (Currentbuf->firstLine == nullptr)
    return;
  while (Currentbuf->currentLine->next && Currentbuf->currentLine->next->bpos)
    cursorDown0(Currentbuf, 1);
  Currentbuf->pos = Currentbuf->currentLine->len - 1;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

static int cur_real_linenumber(Buffer *buf) {
  Line *l, *cur = buf->currentLine;
  int n;

  if (!cur)
    return 1;
  n = cur->real_linenumber ? cur->real_linenumber : 1;
  for (l = buf->firstLine; l && l != cur && l->real_linenumber == 0;
       l = l->next) { /* header */
    if (l->bpos == 0)
      n++;
  }
  return n;
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source") {
  auto fn = Currentbuf->info->filename;

  if (fn == nullptr || /* Behaving as a pager */
      (Currentbuf->info->type == nullptr &&
       Currentbuf->edit == nullptr) || /* Reading shell */
      Currentbuf->real_schema != SCM_LOCAL ||
      !strcmp(Currentbuf->info->currentURL.file, "-") /* file is std input  */
  ) {
    disp_err_message("Can't edit other than local file", true);
    return;
  }

  Str *cmd;
  if (Currentbuf->edit) {
    cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->info->real_type, (char *)fn,
                          (char *)checkHeader(Currentbuf, "Content-Type:"),
                          nullptr);
  } else {
    cmd = myEditor(Editor, shell_quote((char *)fn),
                   cur_real_linenumber(Currentbuf));
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

static Buffer *loadNormalBuf(Buffer *buf, int renderframe) {
  pushBuffer(buf);
  return buf;
}

static Buffer *loadLink(const char *url, const char *target,
                        const char *referer, FormList *request) {
  Buffer *buf, *nfbuf;
  // union frameset_element *f_element = nullptr;
  Url *base, pu;
  const int *no_referer_ptr;

  message(Sprintf("loading %s", url)->ptr, 0, 0);
  refresh(term_io());

  no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->info->currentURL);
  base = baseURL(Currentbuf);
  if ((no_referer_ptr && *no_referer_ptr) || base == nullptr ||
      base->schema == SCM_LOCAL || base->schema == SCM_LOCAL_CGI ||
      base->schema == SCM_DATA)
    referer = NO_REFERER;
  if (referer == nullptr)
    referer = Strnew(Currentbuf->info->currentURL.to_RefererStr())->ptr;
  buf =
      loadGeneralFile(url, baseURL(Currentbuf), {.referer = referer}, request);
  if (buf == nullptr) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
    return nullptr;
  }

  pu = Url::parse2(url, base);
  pushHashHist(URLHist, pu.to_Str().c_str());

  if (buf == NO_BUFFER) {
    return nullptr;
  }
  if (!on_target) /* open link as an indivisual page */
    return loadNormalBuf(buf, true);

  if (do_download) /* download (thus no need to render frames) */
    return loadNormalBuf(buf, false);

  if (target == nullptr || /* no target specified (that means this page is not a
                           frame page) */
      !strcmp(target, "_top") /* this link is specified to be opened as an
                                 indivisual * page */
  ) {
    return loadNormalBuf(buf, true);
  }
  nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
  if (nfbuf == nullptr) {
    /* original page (that contains <frameset> tag) doesn't exist */
    return loadNormalBuf(buf, true);
  }

  {
    Anchor *al = nullptr;
    auto label = pu.label;

    if (!al) {
      label = Strnew_m_charp("_", target, nullptr)->ptr;
      al = searchURLLabel(Currentbuf, label);
    }
    if (al) {
      gotoLine(Currentbuf, al->start.line);
      if (label_topline)
        Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                                       Currentbuf->currentLine->linenumber -
                                           Currentbuf->topLine->linenumber,
                                       false);
      Currentbuf->pos = al->start.pos;
      arrangeCursor(Currentbuf);
    }
  }
  displayBuffer(Currentbuf, B_NORMAL);
  return buf;
}

static void gotoLabel(const char *label) {
  Buffer *buf;
  Anchor *al;
  int i;

  al = searchURLLabel(Currentbuf, label);
  if (al == nullptr) {
    disp_message(Sprintf("%s is not found", label)->ptr, true);
    return;
  }
  buf = new Buffer(Currentbuf->width);
  *buf = *Currentbuf;
  for (i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->info->currentURL.label = allocStr(label, -1);
  pushHashHist(URLHist, buf->info->currentURL.to_Str().c_str());
  buf->clone->count++;
  pushBuffer(buf);
  gotoLine(Currentbuf, al->start.line);
  if (label_topline)
    Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                                   Currentbuf->currentLine->linenumber -
                                       Currentbuf->topLine->linenumber,
                                   false);
  Currentbuf->pos = al->start.pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  return;
}

static int handleMailto(const char *url) {
  Str *to;
  char *pos;

  if (strncasecmp(url, "mailto:", 7))
    return 0;
  if (!non_null(Mailer)) {
    disp_err_message("no mailer is specified", true);
    return 1;
  }

  /* invoke external mailer */
  if (MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
    to = Strnew_charp(html_unquote((char *)url));
  } else {
    to = Strnew_charp(url + 7);
    if ((pos = strchr(to->ptr, '?')) != nullptr)
      Strtruncate(to, pos - to->ptr);
  }
  exec_cmd(
      myExtCommand(Mailer, shell_quote(file_unquote(to->ptr)), false)->ptr);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  pushHashHist(URLHist, (char *)url);
  return 1;
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer") {
  Anchor *a;
  Url u;
  const char *url;

  if (Currentbuf->firstLine == nullptr)
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
  u = Url::parse2(a->url, baseURL(Currentbuf));
  if (u.to_Str() == Currentbuf->info->currentURL.to_Str()) {
    /* index within this buffer */
    if (u.label) {
      gotoLabel(u.label);
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
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  loadLink(url, a->target, a->referer, nullptr);
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
  Anchor *a;
  Buffer *buf;

  if (Currentbuf->firstLine == nullptr)
    return;

  a = retrieveCurrentImg(Currentbuf);
  if (a == nullptr)
    return;
  /* FIXME: gettextize? */
  message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  refresh(term_io());
  buf = loadGeneralFile(a->url, baseURL(Currentbuf), {});
  if (buf == nullptr) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

static FormItemList *save_submit_formlist(FormItemList *src) {
  FormList *list;
  FormList *srclist;
  FormItemList *srcitem;
  FormItemList *item;
  FormItemList *ret = nullptr;

  if (src == nullptr)
    return nullptr;
  srclist = src->parent;
  list = (FormList *)New(FormList);
  list->method = srclist->method;
  list->action = srclist->action->Strdup();
  list->enctype = srclist->enctype;
  list->nitems = srclist->nitems;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
    item = (FormItemList *)New(FormItemList);
    item->type = srcitem->type;
    item->name = srcitem->name->Strdup();
    item->value = srcitem->value->Strdup();
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    item->next = nullptr;

    if (list->lastitem == nullptr) {
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

static void query_from_followform(Str **query, FormItemList *fi,
                                  int multipart) {
  FormItemList *f2;
  FILE *body = nullptr;

  if (multipart) {
    *query = tmpfname(TMPF_DFL, nullptr);
    body = fopen((*query)->ptr, "w");
    if (body == nullptr) {
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
    if (f2->name == nullptr)
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
      if (f2 != fi || f2->value == nullptr)
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
        *query = conv_form_encoding(f2->name, fi, Currentbuf)->Strdup();
        Strcat_charp(*query, ".x");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", x)->ptr);
        *query = conv_form_encoding(f2->name, fi, Currentbuf)->Strdup();
        Strcat_charp(*query, ".y");
        form_write_data(body, fi->parent->boundary, (*query)->ptr,
                        Sprintf("%d", y)->ptr);
      } else if (f2->name && f2->name->length > 0 && f2->value != nullptr) {
        /* not IMAGE */
        *query = conv_form_encoding(f2->value, fi, Currentbuf);
        if (f2->type == FORM_INPUT_FILE)
          form_write_from_file(
              body, fi->parent->boundary,
              conv_form_encoding(f2->name, fi, Currentbuf)->ptr, (*query)->ptr,
              f2->value->ptr);
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
        if (f2->value != nullptr) {
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

static void do_submit(FormItemList *fi, Anchor *a) {
  auto tmp = Strnew();
  auto multipart = (fi->parent->method == FORM_METHOD_POST &&
                    fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
  query_from_followform(&tmp, fi, multipart);

  auto tmp2 = fi->parent->action->Strdup();
  if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
    /* It means "current URL" */
    tmp2 = Strnew(Currentbuf->info->currentURL.to_Str());
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
  }

  if (fi->parent->method == FORM_METHOD_GET) {
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    Strcat_charp(tmp2, "?");
    Strcat(tmp2, tmp);
    loadLink(tmp2->ptr, a->target, nullptr, nullptr);
  } else if (fi->parent->method == FORM_METHOD_POST) {
    Buffer *buf;
    if (multipart) {
      struct stat st;
      stat(fi->parent->body, &st);
      fi->parent->length = st.st_size;
    } else {
      fi->parent->body = tmp->ptr;
      fi->parent->length = tmp->length;
    }
    buf = loadLink(tmp2->ptr, a->target, nullptr, fi->parent);
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
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void _followForm(int submit) {
  if (Currentbuf->firstLine == nullptr)
    return;

  auto a = retrieveCurrentForm(Currentbuf);
  if (a == nullptr)
    return;
  auto fi = (FormItemList *)a->url;

  switch (fi->type) {
  case FORM_INPUT_TEXT:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);
    inputStrHist("TEXT:", fi->value ? fi->value->ptr : nullptr, TextHist,
                 [fi, a](const char *p) {
                   if (p == nullptr || fi->readonly) {
                     return;
                     // break;
                   }
                   fi->value = Strnew_charp(p);
                   formUpdateBuffer(a, Currentbuf, fi);
                   if (fi->accept || fi->parent->nitems == 1) {
                     do_submit(fi, a);
                     return;
                   }
                   displayBuffer(Currentbuf, B_FORCE_REDRAW);
                 });
    break;

  case FORM_INPUT_FILE:
    if (submit) {
      do_submit(fi, a);
      return;
    }

    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);

    // p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : nullptr,
    // nullptr); if (p == nullptr || fi->readonly)
    //   break;
    //
    // fi->value = Strnew_charp(p);
    // formUpdateBuffer(a, Currentbuf, fi);
    // if (fi->accept || fi->parent->nitems == 1)
    //   goto do_submit;
    break;
  case FORM_INPUT_PASSWORD:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    // p = inputLine("Password:", fi->value ? fi->value->ptr : nullptr,
    // IN_PASSWORD);
    // if (p == nullptr)
    //   break;
    // fi->value = Strnew_charp(p);
    formUpdateBuffer(a, Currentbuf, fi);
    if (fi->accept) {
      do_submit(fi, a);
      return;
    }
    break;
  case FORM_TEXTAREA:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);
    input_textarea(fi);
    formUpdateBuffer(a, Currentbuf, fi);
    break;
  case FORM_INPUT_RADIO:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    formRecheckRadio(a, Currentbuf, fi);
    break;
  case FORM_INPUT_CHECKBOX:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(a, Currentbuf, fi);
    break;
  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
    do_submit(fi, a);
    break;

  case FORM_INPUT_RESET:
    for (size_t i = 0; i < Currentbuf->formitem->size(); i++) {
      auto a2 = &Currentbuf->formitem->anchors[i];
      auto f2 = (FormItemList *)a2->url;
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
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (Currentbuf->firstLine == nullptr)
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
    if (Currentbuf->href) {
      an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && Currentbuf->formitem) {
      an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq++;
  } while (an == nullptr);

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink") {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (Currentbuf->firstLine == nullptr)
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
    if (Currentbuf->href) {
      an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
    }
    if (an == nullptr && Currentbuf->formitem) {
      an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
    }
    hseq--;
  } while (an == nullptr);

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link") {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  po = hl->marks + n - 1;
  if (Currentbuf->href) {
    an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr && Currentbuf->formitem) {
    an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
  }
  if (an == nullptr)
    return;

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
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
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  Url url;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  y = Currentbuf->currentLine->linenumber;
  x = Currentbuf->pos;

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
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq++;
        if (visited == true && an) {
          url = Url::parse2(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_next_anchor(Currentbuf->href, nullptr, x, y);
      if (visited != true)
        an = closest_next_anchor(Currentbuf->formitem, an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true) {
        url = Url::parse2(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  po = &hl->marks[an->hseq];
  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous anchor */
static void _prevA(int visited) {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  Url url;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (visited != true && an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  y = Currentbuf->currentLine->linenumber;
  x = Currentbuf->pos;

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
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(po->line, po->pos);
        }
        if (visited != true && an == nullptr && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(po->line, po->pos);
        }
        hseq--;
        if (visited == true && an) {
          url = Url::parse2(an->url, baseURL(Currentbuf));
          if (getHashHist(URLHist, url.to_Str().c_str())) {
            goto _end;
          }
        }
      } while (an == nullptr || an == pan);
    } else {
      an = closest_prev_anchor(Currentbuf->href, nullptr, x, y);
      if (visited != true)
        an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
      if (an == nullptr) {
        if (visited == true)
          return;
        an = pan;
        break;
      }
      x = an->start.pos;
      y = an->start.line;
      if (visited == true && an) {
        url = Url::parse2(an->url, baseURL(Currentbuf));
        if (getHashHist(URLHist, url.to_Str().c_str())) {
          goto _end;
        }
      }
    }
  }
  if (visited == true)
    return;

_end:
  if (an == nullptr || an->hseq < 0)
    return;
  po = hl->marks + an->hseq;
  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left/right anchor */
static void nextX(int d, int dy) {
  HmarkerList *hl = Currentbuf->hmarklist;
  Anchor *an, *pan;
  Line *l;
  int i, x, y, n = searchKeyNum();

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  l = Currentbuf->currentLine;
  x = Currentbuf->pos;
  y = l->linenumber;
  pan = nullptr;
  for (i = 0; i < n; i++) {
    if (an)
      x = (d > 0) ? an->end.pos : an->start.pos - 1;
    an = nullptr;
    while (1) {
      for (; x >= 0 && x < l->len; x += d) {
        if (Currentbuf->href) {
          an = Currentbuf->href->retrieveAnchor(y, x);
        }
        if (!an && Currentbuf->formitem) {
          an = Currentbuf->formitem->retrieveAnchor(y, x);
        }
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

  if (pan == nullptr)
    return;
  gotoLine(Currentbuf, y);
  Currentbuf->pos = pan->start.pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next downward/upward anchor */
static void nextY(int d) {
  HmarkerList *hl = Currentbuf->hmarklist;
  Anchor *an, *pan;
  int i, x, y, n = searchKeyNum();
  int hseq;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (!hl || hl->nmark == 0)
    return;

  an = retrieveCurrentAnchor(Currentbuf);
  if (an == nullptr)
    an = retrieveCurrentForm(Currentbuf);

  x = Currentbuf->pos;
  y = Currentbuf->currentLine->linenumber + d;
  pan = nullptr;
  hseq = -1;
  for (i = 0; i < n; i++) {
    if (an)
      hseq = abs(an->hseq);
    an = nullptr;
    for (; y >= 0 && y <= Currentbuf->lastLine->linenumber; y += d) {
      if (Currentbuf->href) {
        an = Currentbuf->href->retrieveAnchor(y, x);
      }
      if (!an && Currentbuf->formitem) {
        an = Currentbuf->formitem->retrieveAnchor(y, x);
      }
      if (an && hseq != abs(an->hseq)) {
        pan = an;
        break;
      }
    }
    if (!an)
      break;
  }

  if (pan == nullptr)
    return;
  gotoLine(Currentbuf, pan->start.line);
  arrangeLine(Currentbuf);
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

static int checkBackBuffer(Buffer *buf) {
  if (buf->nextBuffer)
    return true;

  return false;
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

  delBuffer(Currentbuf);

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local CGI-scripts)") {
  Buffer *buf = Currentbuf->nextBuffer;
  if (buf)
    delBuffer(buf);
}

static void cmd_loadURL(const char *url, Url *current, const char *referer,
                        FormList *request) {
  if (handleMailto((char *)url))
    return;

  refresh(term_io());
  auto buf = loadGeneralFile(url, current, {.referer = referer}, request);
  if (buf == nullptr) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to specified URL */
static void goURL0(const char *prompt, int relative) {
  const char *url, *referer;
  Url p_url, *current;
  Buffer *cur_buf = Currentbuf;
  const int *no_referer_ptr;

  url = searchKeyData();
  if (url == nullptr) {
    Hist *hist = copyHist(URLHist);
    Anchor *a;

    current = baseURL(Currentbuf);
    if (current) {
      auto c_url = current->to_Str();
      if (DefaultURLString == DEFAULT_URL_CURRENT)
        url = url_decode0(c_url.c_str());
      else
        pushHist(hist, c_url);
    }
    a = retrieveCurrentAnchor(Currentbuf);
    if (a) {
      p_url = Url::parse2(a->url, current);
      auto a_url = p_url.to_Str();
      if (DefaultURLString == DEFAULT_URL_LINK)
        url = url_decode0(a_url.c_str());
      else
        pushHist(hist, a_url);
    }
    // url = inputLineHist(prompt, url, IN_URL, hist);
    if (url != nullptr)
      SKIP_BLANKS(url);
  }
  if (relative) {
    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->info->currentURL);
    current = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || current == nullptr ||
        current->schema == SCM_LOCAL || current->schema == SCM_LOCAL_CGI ||
        current->schema == SCM_DATA)
      referer = NO_REFERER;
    else
      referer = Strnew(Currentbuf->info->currentURL.to_RefererStr())->ptr;
    url = url_quote((char *)url);
  } else {
    current = nullptr;
    referer = nullptr;
    url = url_quote((char *)url);
  }
  if (url == nullptr || *url == '\0') {
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  if (*url == '#') {
    gotoLabel(url + 1);
    return;
  }
  p_url = Url::parse2(url, current);
  pushHashHist(URLHist, p_url.to_Str().c_str());
  cmd_loadURL(url, current, referer, nullptr);
  if (Currentbuf != cur_buf) /* success */
    pushHashHist(URLHist, Currentbuf->info->currentURL.to_Str().c_str());
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
    url = url_quote(url);
    p_url = Url::parse2(url);
    pushHashHist(URLHist, p_url.to_Str().c_str());
    cmd_loadURL(url, nullptr, nullptr, nullptr);
    if (Currentbuf != cur_buf) /* success */
      pushHashHist(URLHist, Currentbuf->info->currentURL.to_Str().c_str());
  }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address") {
  goURL0("Goto relative URL: ", true);
}

static void cmd_loadBuffer(Buffer *buf, int prop, int linkid) {
  if (buf == nullptr) {
    disp_err_message("Can't load string", false);
  } else if (buf != NO_BUFFER) {
    buf->bufferprop = (BufferFlags)(buf->bufferprop | BP_INTERNAL | prop);
    if (!(buf->bufferprop & BP_NO_URL))
      buf->info->currentURL = Currentbuf->info->currentURL;
    if (linkid != LB_NOLINK) {
      buf->linkBuffer[linkid] = Currentbuf;
      Currentbuf->linkBuffer[linkid] = buf;
    }
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks") {
  cmd_loadURL(BookmarkFile, nullptr, NO_REFERER, nullptr);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks") {
  Str *tmp;
  FormList *request;

  tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s",
                (Str_form_quote(localCookie()))->ptr,
                (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
                (Str_form_quote(Strnew(Currentbuf->info->currentURL.to_Str())))->ptr,
                (Str_form_quote(Strnew_charp(Currentbuf->buffername)))->ptr);
  request =
      newFormList(nullptr, "post", nullptr, nullptr, nullptr, nullptr, nullptr);
  request->body = tmp->ptr;
  request->length = tmp->length;
  cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, nullptr, NO_REFERER,
              request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel") {
  cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
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
  cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document") {
  Buffer *buf;

  if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != nullptr) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if ((buf = Currentbuf->linkBuffer[LB_INFO]) != nullptr)
    delBuffer(buf);
  buf = page_info_panel(Currentbuf);
  cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced") {
  Buffer *buf;

  buf = link_list_panel(Currentbuf);
  if (buf != nullptr) {
    cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
  }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list") {
  Buffer *buf;

  buf = cookie_list_panel();
  if (buf != nullptr)
    cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history") {
  cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
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
  const char *file;

  if (Currentbuf->sourcefile == nullptr)
    return;
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  PermitSaveToPipe = true;
  if (Currentbuf->real_schema == SCM_LOCAL)
    file = guess_save_name(nullptr, (char *)Currentbuf->info->currentURL.real_file);
  else
    file = guess_save_name(Currentbuf, Currentbuf->info->currentURL.file);
  doFileCopy(Currentbuf->sourcefile, file);
  PermitSaveToPipe = false;
  displayBuffer(Currentbuf, B_NORMAL);
}

static void _peekURL(int only_img) {

  Anchor *a;
  Url pu;
  static Str *s = nullptr;
  static int offset = 0, n;

  if (Currentbuf->firstLine == nullptr)
    return;
  if (CurrentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = nullptr;
  a = (only_img ? nullptr : retrieveCurrentAnchor(Currentbuf));
  if (a == nullptr) {
    a = (only_img ? nullptr : retrieveCurrentForm(Currentbuf));
    if (a == nullptr) {
      a = retrieveCurrentImg(Currentbuf);
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    pu = Url::parse2(a->url, baseURL(Currentbuf));
    s = Strnew(pu.to_Str());
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message(&s->ptr[offset], true);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address") { _peekURL(0); }

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address") { _peekURL(1); }

/* show current URL */
static Str *currentURL(void) {
  if (Currentbuf->bufferprop & BP_INTERNAL)
    return Strnew_size(0);
  return Strnew(Currentbuf->info->currentURL.to_Str());
}

DEFUN(curURL, PEEK, "Show current address") {
  static Str *s = nullptr;
  static int offset = 0, n;

  if (Currentbuf->bufferprop & BP_INTERNAL)
    return;
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
  if (Currentbuf->sourcefile == nullptr) {
    return;
  }

  buf = new Buffer(INIT_BUFFER_WIDTH());

  if (is_html_type(Currentbuf->info->type)) {
    buf->info->type = "text/plain";
    if (Currentbuf->info->real_type && is_html_type(Currentbuf->info->real_type))
      buf->info->real_type = "text/plain";
    else
      buf->info->real_type = Currentbuf->info->real_type;
    buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
    buf->linkBuffer[LB_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_SOURCE] = buf;
  } else if (!strcasecmp(Currentbuf->info->type, "text/plain")) {
    buf->info->type = "text/html";
    if (Currentbuf->info->real_type &&
        !strcasecmp(Currentbuf->info->real_type, "text/plain"))
      buf->info->real_type = "text/html";
    else
      buf->info->real_type = Currentbuf->info->real_type;
    buf->buffername = Sprintf("HTML view of %s", Currentbuf->buffername)->ptr;
    buf->linkBuffer[LB_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_SOURCE] = buf;
  } else {
    return;
  }
  buf->info->currentURL = Currentbuf->info->currentURL;
  buf->real_schema = Currentbuf->real_schema;
  buf->info->filename = Currentbuf->info->filename;
  buf->sourcefile = Currentbuf->sourcefile;
  buf->clone = Currentbuf->clone;
  buf->clone->count++;

  buf->need_reshape = true;
  reshapeBuffer(buf);
  pushBuffer(buf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew") {
  Str *url;
  FormList *request;
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
  if (Currentbuf->info->currentURL.schema == SCM_LOCAL &&
      !strcmp(Currentbuf->info->currentURL.file, "-")) {
    /* file is std input */
    /* FIXME: gettextize? */
    disp_err_message("Can't reload stdin", true);
    return;
  }
  auto sbuf = new Buffer(0);
  *sbuf = *Currentbuf;

  multipart = 0;
  if (Currentbuf->form_submit) {
    request = Currentbuf->form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      Str *query;
      struct stat st;
      multipart = 1;
      query_from_followform(&query, Currentbuf->form_submit, multipart);
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
      url->ptr, nullptr, {.referer = NO_REFERER, .no_cache = true}, request);
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
      ((!strcasecmp(buf->info->type, "text/plain") && is_html_type(sbuf->info->type)) ||
       (is_html_type(buf->info->type) && !strcasecmp(sbuf->info->type, "text/plain")))) {
    vwSrc();
    if (Currentbuf != buf)
      Firstbuf = deleteBuffer(Firstbuf, buf);
  }
  Currentbuf->form_submit = sbuf->form_submit;
  if (Currentbuf->firstLine) {
    COPY_BUFROOT(Currentbuf, sbuf);
    restorePosition(Currentbuf, sbuf);
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
void chkURLBuffer(Buffer *buf) {
  static const char *url_like_pat[] = {
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
      nullptr};
  int i;
  for (i = 0; url_like_pat[i]; i++) {
    reAnchor(buf, url_like_pat[i]);
  }
  buf->check_url = true;
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
  reAnchorWord(Currentbuf, Currentbuf->currentLine, spos, epos);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames") {}

/* spawn external browser */
static void invoke_browser(const char *url) {
  Str *cmd;
  const char *browser = nullptr;
  int bg = 0, len;

  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  browser = searchKeyData();
  if (browser == nullptr || *browser == '\0') {
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
    if (browser == nullptr || *browser == '\0') {
      // browser = inputStr("Browse command: ", nullptr);
    }
  }
  if (browser == nullptr || *browser == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }

  if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' &&
      browser[len - 2] != '\\') {
    browser = allocStr(browser, len - 2);
    bg = 1;
  }
  cmd = myExtCommand((char *)browser, shell_quote(url), false);
  Strremovetrailingspaces(cmd);
  fmTerm();
  mySystem(cmd->ptr, bg);
  fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(extbrz, EXTERN, "Display using an external browser") {
  if (Currentbuf->bufferprop & BP_INTERNAL) {
    disp_err_message("Can't browse...", true);
    return;
  }
  if (Currentbuf->info->currentURL.schema == SCM_LOCAL &&
      !strcmp(Currentbuf->info->currentURL.file, "-")) {
    /* file is std input */
    disp_err_message("Can't browse stdin", true);
    return;
  }
  invoke_browser(Currentbuf->info->currentURL.to_Str().c_str());
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser") {
  if (Currentbuf->firstLine == nullptr)
    return;
  auto a = retrieveCurrentAnchor(Currentbuf);
  if (a == nullptr)
    return;
  auto pu = Url::parse2(a->url, baseURL(Currentbuf));
  invoke_browser(pu.to_Str().c_str());
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document") {
  Line *l = Currentbuf->currentLine;
  Str *tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != nullptr) {
    cur = l->real_linenumber;
    col = l->bwidth + Currentbuf->currentColumn + Currentbuf->cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    len = l->bwidth + l->width();
  }
  if (Currentbuf->lastLine)
    all = Currentbuf->lastLine->real_linenumber;
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

static void execdict(const char *word) {
  const char *w, *dictcmd;
  Buffer *buf;

  if (!UseDictCommand || word == nullptr || *word == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  w = word;
  if (*w == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  dictcmd =
      Sprintf("%s?%s", DictCommand, Str_form_quote(Strnew_charp(w))->ptr)->ptr;
  buf = loadGeneralFile(dictcmd, nullptr, {.referer = NO_REFERER});
  if (buf == nullptr) {
    disp_message("Execution failed", true);
    return;
  } else if (buf != NO_BUFFER) {
    buf->info->filename = w;
    buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->info->type == nullptr)
      buf->info->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)") {
  // execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
      "Execute dictionary command for word at cursor") {
  execdict(GetWord(Currentbuf));
}

static int searchKeyNum(void) {

  auto d = searchKeyData();
  int n = 1;
  if (d != nullptr)
    n = atoi(d);
  return n * PREC_NUM;
}

void deleteFiles() {
  Buffer *buf;
  char *f;

  for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
    while (Firstbuf && Firstbuf != NO_BUFFER) {
      buf = Firstbuf->nextBuffer;
      discardBuffer(Firstbuf);
      Firstbuf = buf;
    }
  }
  while ((f = popText(fileToDelete)) != nullptr) {
    unlink(f);
  }
}

void w3m_exit(int i) {
  stopDownload();
  deleteFiles();
  free_ssl_ctx();
  if (mkd_tmp_dir)
    if (rmdir(mkd_tmp_dir) != 0) {
      fprintf(stderr, "Can't remove temporary directory (%s)!\n", mkd_tmp_dir);
      exit(1);
    }
  exit(i);
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

AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, int cmd,
                          void *data) {
  if (event == nullptr)
    event = (AlarmEvent *)New(AlarmEvent);
  event->sec = sec;
  event->status = status;
  event->cmd = cmd;
  event->data = data;
  return event;
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

TabBuffer *newTab(void) {

  auto n = (TabBuffer *)New(TabBuffer);
  if (n == nullptr)
    return nullptr;
  n->nextTab = nullptr;
  n->currentBuffer = nullptr;
  n->firstBuffer = nullptr;
  return n;
}

static void _newT(void) {
  TabBuffer *tag;
  Buffer *buf;
  int i;

  tag = newTab();
  if (!tag)
    return;

  buf = new Buffer(Currentbuf->width);
  *buf = *Currentbuf;
  buf->nextBuffer = nullptr;
  for (i = 0; i < MAX_LB; i++)
    buf->linkBuffer[i] = nullptr;
  buf->clone->count++;
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

static TabBuffer *numTab(int n) {
  TabBuffer *tab;
  int i;

  if (n == 0)
    return CurrentTab;
  if (n == 1)
    return FirstTab;
  if (nTab <= 1)
    return nullptr;
  for (tab = FirstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void calcTabPos(void) {
  TabBuffer *tab;
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

TabBuffer *deleteTab(TabBuffer *tab) {
  Buffer *buf, *next;

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
    tab->nextTab->prevTab = nullptr;
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

static void followTab(TabBuffer *tab) {
  Buffer *buf;
  Anchor *a;

  a = retrieveCurrentAnchor(Currentbuf);
  if (a == nullptr)
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
  if (tab == nullptr) {
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    Buffer *c, *p;

    c = Currentbuf;
    if ((p = prevBuffer(c, buf)))
      p->nextBuffer = nullptr;
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
  followTab(prec_num ? numTab(PREC_NUM) : nullptr);
}

static void tabURL0(TabBuffer *tab, const char *prompt, int relative) {
  Buffer *buf;

  if (tab == CurrentTab) {
    goURL0(prompt, relative);
    return;
  }
  _newT();
  buf = Currentbuf;
  goURL0(prompt, relative);
  if (tab == nullptr) {
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
  } else if (buf != Currentbuf) {
    /* buf <- p <- ... <- Currentbuf = c */
    Buffer *c, *p;

    c = Currentbuf;
    if ((p = prevBuffer(c, buf)))
      p->nextBuffer = nullptr;
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
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto URL on new tab: ", false);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : nullptr,
          "Goto relative URL on new tab: ", true);
}

static void moveTab(TabBuffer *t, TabBuffer *t2, int right) {
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
    t->nextTab->prevTab = nullptr;
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

static char *convert_size3(long long size) {
  Str *tmp = Strnew();
  int n;

  do {
    n = size % 1000;
    size /= 1000;
    tmp = Sprintf(size ? ",%.3d%s" : "%d%s", n, tmp->ptr);
  } while (size);
  return tmp->ptr;
}

static Buffer *DownloadListBuffer(void) {
  DownloadList *d;
  Str *src = nullptr;
  struct stat st;
  time_t cur_time;
  int duration, rate, eta;
  size_t size;

  if (!FirstDL)
    return nullptr;
  cur_time = time(0);
  /* FIXME: gettextize? */
  src = Strnew_charp(
      "<html><head><title>" DOWNLOAD_LIST_TITLE
      "</title></head>\n<body><h1 align=center>" DOWNLOAD_LIST_TITLE "</h1>\n"
      "<form method=internal action=download><hr>\n");
  for (d = LastDL; d != nullptr; d = d->prev) {
    if (lstat(d->lock, &st))
      d->running = false;
    Strcat_charp(src, "<pre>\n");
    Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
                        html_quote(d->save)));
    duration = cur_time - d->time;
    if (!stat(d->save, &st)) {
      size = st.st_size;
      if (!d->running) {
        if (!d->err)
          d->size = size;
        duration = st.st_mtime - d->time;
      }
    } else
      size = 0;
    if (d->size) {
      int i, l = COLS - 6;
      if (size < d->size)
        i = 1.0 * l * size / d->size;
      else
        i = l;
      l -= i;
      while (i-- > 0)
        Strcat_char(src, '#');
      while (l-- > 0)
        Strcat_char(src, '_');
      Strcat_char(src, '\n');
    }
    if ((d->running || d->err) && size < d->size)
      Strcat(src,
             Sprintf("  %s / %s bytes (%d%%)", convert_size3(size),
                     convert_size3(d->size), (int)(100.0 * size / d->size)));
    else
      Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
    if (duration > 0) {
      rate = size / duration;
      Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec", duration / (60 * 60),
                          (duration / 60) % 60, duration % 60,
                          convert_size(rate, 1)));
      if (d->running && size < d->size && rate) {
        eta = (d->size - size) / rate;
        Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                            (eta / 60) % 60, eta % 60));
      }
    }
    Strcat_char(src, '\n');
    if (!d->running) {
      Strcat(src, Sprintf("<input type=submit name=ok%d value=OK>", d->pid));
      switch (d->err) {
      case 0:
        if (size < d->size)
          Strcat_charp(src, " Download ended but probably not complete");
        else
          Strcat_charp(src, " Download complete");
        break;
      case 1:
        Strcat_charp(src, " Error: could not open destination file");
        break;
      case 2:
        Strcat_charp(src, " Error: could not write to file (disk full)");
        break;
      default:
        Strcat_charp(src, " Error: unknown reason");
      }
    } else
      Strcat(src,
             Sprintf("<input type=submit name=stop%d value=STOP>", d->pid));
    Strcat_charp(src, "\n</pre><hr>\n");
  }
  Strcat_charp(src, "</form></body></html>");
  return loadHTMLString(src);
}

void download_action(struct keyvalue *arg) {
  DownloadList *d;
  pid_t pid;

  for (; arg; arg = arg->next) {
    if (!strncmp(arg->arg, "stop", 4)) {
      pid = (pid_t)atoi(&arg->arg[4]);
      kill(pid, SIGKILL);
    } else if (!strncmp(arg->arg, "ok", 2))
      pid = (pid_t)atoi(&arg->arg[2]);
    else
      continue;
    for (d = FirstDL; d; d = d->next) {
      if (d->pid == pid) {
        unlink(d->lock);
        if (d->prev)
          d->prev->next = d->next;
        else
          FirstDL = d->next;
        if (d->next)
          d->next->prev = d->prev;
        else
          LastDL = d->prev;
        break;
      }
    }
  }
  ldDL();
}

void stopDownload(void) {
  DownloadList *d;

  if (!FirstDL)
    return;
  for (d = FirstDL; d != nullptr; d = d->next) {
    if (!d->running)
      continue;
    kill(d->pid, SIGKILL);
    unlink(d->lock);
  }
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel") {
  Buffer *buf;
  int replace = false, new_tab = false;
  int reload;

  if (Currentbuf->bufferprop & BP_INTERNAL &&
      !strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
    replace = true;
  if (!FirstDL) {
    if (replace) {
      if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == nullptr) {
        if (nTab > 1)
          deleteTab(CurrentTab);
      } else
        delBuffer(Currentbuf);
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
  buf->bufferprop = (BufferFlags)(buf->bufferprop | BP_INTERNAL | BP_NO_URL);
  if (replace) {
    COPY_BUFROOT(buf, Currentbuf);
    restorePosition(buf, Currentbuf);
  }
  if (!replace && open_tab_dl_list) {
    _newT();
    new_tab = true;
  }
  pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf();
  if (reload)
    Currentbuf->event = setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT,
                                      FUNCNAME_reload, nullptr);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement") {
  BufferPos *b = Currentbuf->undo;
  int i;

  if (!Currentbuf->firstLine)
    return;
  if (!b || !b->prev)
    return;
  for (i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
    ;
  resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo") {
  BufferPos *b = Currentbuf->undo;
  int i;

  if (!Currentbuf->firstLine)
    return;
  if (!b || !b->next)
    return;
  for (i = 0; i < PREC_NUM && b->next; i++, b = b->next)
    ;
  resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen") {
  if (Currentbuf->firstLine == nullptr)
    return;
  Currentbuf->currentLine = lineSkip(Currentbuf, Currentbuf->topLine, 0, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen") {
  int offsety;
  if (Currentbuf->firstLine == nullptr)
    return;
  offsety = (Currentbuf->LINES - 1) / 2;
  Currentbuf->currentLine =
      currentLineSkip(Currentbuf, Currentbuf->topLine, offsety, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen") {
  int offsety;
  if (Currentbuf->firstLine == nullptr)
    return;
  offsety = Currentbuf->LINES - 1;
  Currentbuf->currentLine =
      currentLineSkip(Currentbuf, Currentbuf->topLine, offsety, false);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

int main(int argc, char **argv) {
  Buffer *newbuf = nullptr;
  char *p;
  int i;
  // input_stream *redin;
  char *line_str = nullptr;
  const char **load_argv;
  FormList *request;
  int load_argc = 0;
  int load_bookmark = false;
  int visual_start = false;
  int open_new_tab = false;
  char *default_type = nullptr;
  char *post_file = nullptr;
  Str *err_msg;
  if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
    set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif

  NO_proxy_domains = newTextList();
  fileToDelete = newTextList();

  load_argv = (const char **)New_N(char *, argc - 1);
  load_argc = 0;

  CurrentDir = currentdir();
  CurrentPid = (int)getpid();
  BookmarkFile = nullptr;
  config_file = nullptr;

  {
    char hostname[HOST_NAME_MAX + 2];
    if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
      size_t hostname_len;
      /* Don't use hostname if it is truncated.  */
      hostname[HOST_NAME_MAX + 1] = '\0';
      hostname_len = strlen(hostname);
      if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
        HostName = allocStr(hostname, (int)hostname_len);
    }
  }

  /* argument search 1 */
  for (i = 1; i < argc; i++) {
    if (*argv[i] == '-') {
      if (!strcmp("-config", argv[i])) {
        argv[i] = (char *)"-dummy";
        if (++i >= argc)
          usage();
        config_file = argv[i];
        argv[i] = (char *)"-dummy";
      } else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
        help();
      else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
        fversion(stdout);
        exit(0);
      }
    }
  }

  /* initializations */
  init_rc();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();

  if (!non_null(HTTP_proxy) &&
      ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) ||
       (p = getenv("HTTP_proxy"))))
    HTTP_proxy = p;
  if (!non_null(HTTPS_proxy) &&
      ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) ||
       (p = getenv("HTTPS_proxy"))))
    HTTPS_proxy = p;
  if (HTTPS_proxy == nullptr && non_null(HTTP_proxy))
    HTTPS_proxy = HTTP_proxy;
  if (!non_null(FTP_proxy) &&
      ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) ||
       (p = getenv("FTP_proxy"))))
    FTP_proxy = p;
  if (!non_null(NO_proxy) &&
      ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) ||
       (p = getenv("NO_proxy"))))
    NO_proxy = p;

  if (!non_null(Editor) && (p = getenv("EDITOR")) != nullptr)
    Editor = p;
  if (!non_null(Mailer) && (p = getenv("MAILER")) != nullptr)
    Mailer = p;

  /* argument search 2 */
  i = 1;
  while (i < argc) {
    if (*argv[i] == '-') {
      if (!strcmp("-t", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          Tabstop = atoi(argv[i]);
      } else if (!strcmp("-l", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          PagerMax = atoi(argv[i]);
      } else if (!strcmp("-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_DEC;
      else if (!strcmp("-no-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_ASCII;
      else if (!strcmp("-T", argv[i])) {
        if (++i >= argc)
          usage();
        DefaultType = default_type = argv[i];
      } else if (!strcmp("-v", argv[i]))
        visual_start = true;
      else if (!strcmp("-N", argv[i]))
        open_new_tab = true;
      else if (!strcmp("-B", argv[i]))
        load_bookmark = true;
      else if (!strcmp("-bookmark", argv[i])) {
        if (++i >= argc)
          usage();
        BookmarkFile = argv[i];
        if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
          Str *tmp = Strnew_charp(CurrentDir);
          if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
          Strcat_charp(tmp, BookmarkFile);
          BookmarkFile = cleanupName(tmp->ptr);
        }
      } else if (!strcmp("-W", argv[i])) {
        if (WrapDefault)
          WrapDefault = false;
        else
          WrapDefault = true;
      } else if (!strcmp("-cols", argv[i])) {
        if (++i >= argc)
          usage();
        COLS = atoi(argv[i]);
        if (COLS > MAXIMUM_COLS) {
          COLS = MAXIMUM_COLS;
        }
      } else if (!strcmp("-ppc", argv[i])) {
        double ppc;
        if (++i >= argc)
          usage();
        ppc = atof(argv[i]);
        if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
          pixel_per_char = ppc;
          set_pixel_per_char = true;
        }
      } else if (!strcmp("-num", argv[i]))
        showLineNum = true;
      else if (!strcmp("-no-proxy", argv[i]))
        use_proxy = false;
#ifdef INET6
      else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
        set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
#endif
      else if (!strcmp("-post", argv[i])) {
        if (++i >= argc)
          usage();
        post_file = argv[i];
      } else if (!strcmp("-header", argv[i])) {
        Str *hs;
        if (++i >= argc)
          usage();
        if ((hs = make_optional_header_string(argv[i])) != nullptr) {
          if (header_string == nullptr)
            header_string = hs;
          else
            Strcat(header_string, hs);
        }
        while (argv[i][0]) {
          argv[i][0] = '\0';
          argv[i]++;
        }
      } else if (!strcmp("-no-cookie", argv[i])) {
        use_cookie = false;
        accept_cookie = false;
      } else if (!strcmp("-cookie", argv[i])) {
        use_cookie = true;
        accept_cookie = true;
      }
#if 1 /* pager requires -s */
      else if (!strcmp("-s", argv[i]))
#else
      else if (!strcmp("-S", argv[i]))
#endif
        squeezeBlankLine = true;
      else if (!strcmp("-X", argv[i]))
        Do_not_use_ti_te = true;
      else if (!strcmp("-title", argv[i]))
        displayTitleTerm = getenv("TERM");
      else if (!strncmp("-title=", argv[i], 7))
        displayTitleTerm = argv[i] + 7;
      else if (!strcmp("-insecure", argv[i])) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
        set_param_option("ssl_cipher=ALL:enullptr:@SECLEVEL=0");
#else
        set_param_option("ssl_cipher=ALL:enullptr");
#endif
#ifdef SSL_CTX_set_min_proto_version
        set_param_option("ssl_min_version=all");
#endif
        set_param_option("ssl_forbid_method=");
        set_param_option("ssl_verify_server=0");
      } else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i])) {
        if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
            !strcmp(argv[i], "?")) {
          show_params(stdout);
          exit(0);
        }
        if (!set_param_option(argv[i])) {
          /* option set failed */
          /* FIXME: gettextize? */
          fprintf(stderr, "%s: bad option\n", argv[i]);
          show_params_p = 1;
          usage();
        }
      } else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i])) {
        /* do nothing */
      } else if (!strcmp("-reqlog", argv[i])) {
        w3m_reqlog = rcFile("request.log");
      } else {
        usage();
      }
    } else if (*argv[i] == '+') {
      line_str = argv[i] + 1;
    } else {
      load_argv[load_argc++] = argv[i];
    }
    i++;
  }

  FirstTab = nullptr;
  LastTab = nullptr;
  nTab = 0;
  CurrentTab = nullptr;
  CurrentKey = -1;
  if (BookmarkFile == nullptr)
    BookmarkFile = rcFile(BOOKMARK);

  fmInit();
  mySignal(SIGWINCH, resize_hook);

  sync_with_option();
  initCookie();
  if (UseHistory)
    loadHistory(URLHist);

  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif
  err_msg = Strnew();
  if (load_argc == 0) {
    /* no URL specified */
    if (load_bookmark) {
      newbuf = loadGeneralFile(BookmarkFile, nullptr, {.referer = NO_REFERER});
      if (newbuf == nullptr)
        Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
    } else if (visual_start) {
      Str *s_page;
      s_page =
          Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
      Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
      Strcat_m_charp(
          s_page, "w3m</a>!<p><p>This is w3m version ", w3m_version,
          "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
          nullptr);
      newbuf = loadHTMLString(s_page);
      if (newbuf == nullptr)
        Strcat_charp(err_msg, "w3m: Can't load string.\n");
      else if (newbuf != NO_BUFFER)
        newbuf->bufferprop =
            (BufferFlags)(newbuf->bufferprop | BP_INTERNAL | BP_NO_URL);
    } else if ((p = getenv("HTTP_HOME")) != nullptr ||
               (p = getenv("WWW_HOME")) != nullptr) {
      newbuf = loadGeneralFile(p, nullptr, {.referer = NO_REFERER});
      if (newbuf == nullptr)
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
      else if (newbuf != NO_BUFFER)
        pushHashHist(URLHist, newbuf->info->currentURL.to_Str().c_str());
    } else {
      if (fmInitialized)
        fmTerm();
      usage();
    }
    if (newbuf == nullptr) {
      if (fmInitialized)
        fmTerm();
      if (err_msg->length)
        fprintf(stderr, "%s", err_msg->ptr);
      w3m_exit(2);
    }
    i = -1;
  } else {
    i = 0;
  }
  for (; i < load_argc; i++) {
    if (i >= 0) {
      DefaultType = default_type;
      int retry = 0;

      auto url = load_argv[i];
      if (parseUrlSchema(&url) == SCM_MISSING && !ArgvIsURL)
      retry_as_local_file:
        url = file_to_url((char *)load_argv[i]);
      else
        url = url_quote((char *)load_argv[i]);
      {
        if (post_file && i == 0) {
          FILE *fp;
          Str *body;
          if (!strcmp(post_file, "-"))
            fp = stdin;
          else
            fp = fopen(post_file, "r");
          if (fp == nullptr) {
            Strcat(err_msg, Sprintf("w3m: Can't open %s.\n", post_file));
            continue;
          }
          body = Strfgetall(fp);
          if (fp != stdin)
            fclose(fp);
          request = newFormList(nullptr, "post", nullptr, nullptr, nullptr,
                                nullptr, nullptr);
          request->body = body->ptr;
          request->boundary = nullptr;
          request->length = body->length;
        } else {
          request = nullptr;
        }
        newbuf =
            loadGeneralFile(url, nullptr, {.referer = NO_REFERER}, request);
      }
      if (newbuf == nullptr) {
        if (ArgvIsURL && !retry) {
          retry = 1;
          goto retry_as_local_file;
        }
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", load_argv[i]));
        continue;
      } else if (newbuf == NO_BUFFER)
        continue;
      switch (newbuf->real_schema) {
      case SCM_MAILTO:
        break;
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
      default:
        pushHashHist(URLHist, newbuf->info->currentURL.to_Str().c_str());
        break;
      }
    } else if (newbuf == NO_BUFFER)
      continue;
    if (CurrentTab == nullptr) {
      FirstTab = LastTab = CurrentTab = newTab();
      if (!FirstTab) {
        fprintf(stderr, "%s\n", "Can't allocated memory");
        exit(1);
      }
      nTab = 1;
      Firstbuf = Currentbuf = newbuf;
    } else if (open_new_tab) {
      _newT();
      Currentbuf->nextBuffer = newbuf;
      delBuffer(Currentbuf);
    } else {
      Currentbuf->nextBuffer = newbuf;
      Currentbuf = newbuf;
    }
    {
      Currentbuf = newbuf;
      saveBufferInfo();
    }
  }

  if (popAddDownloadList()) {
    CurrentTab = LastTab;
    if (!FirstTab) {
      FirstTab = LastTab = CurrentTab = newTab();
      nTab = 1;
    }
    if (!Firstbuf || Firstbuf == NO_BUFFER) {
      Firstbuf = Currentbuf = new Buffer(INIT_BUFFER_WIDTH());
      Currentbuf->bufferprop = (BufferFlags)(BP_INTERNAL | BP_NO_URL);
      Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
    } else
      Currentbuf = Firstbuf;
    ldDL();
  } else
    CurrentTab = FirstTab;
  if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
    if (newbuf == NO_BUFFER) {
      if (fmInitialized) {
        // inputChar("Hit any key to quit w3m:");
      }
    }
    if (fmInitialized)
      fmTerm();
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    if (newbuf == NO_BUFFER) {
      save_cookies();
      if (!err_msg->length)
        w3m_exit(0);
    }
    w3m_exit(2);
  }
  if (err_msg->length)
    disp_message_nsec(err_msg->ptr, false, 1, true, false);

  DefaultType = nullptr;

  Currentbuf = Firstbuf;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  if (line_str) {
    _goLine(line_str);
  }

  mainLoop();
}
