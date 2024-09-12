/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "tty.h"
#include "scr.h"
#include "termseq/termcap_interface.h"
#include "termcap.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#include <string.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/ioctl.h>

#include "fm.h"

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler reset_error_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

char gcmap[96];

void term_puts(const char *s) { tputs(s, 1, tty_putc); }

#define W3M_TERM_INFO(name, title, mouse) name, title

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

#define MAX_LINE 200
#define MAX_COLUMN 400
int LINES;
int COLS;

void term_setlinescols() {
  char *p;
  int i;
  if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    LINES = i;
  if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    COLS = i;
  if (LINES <= 0)
    LINES = termcap_int_line();
  if (COLS <= 0)
    COLS = termcap_int_cols();
  if (COLS > MAX_COLUMN)
    COLS = MAX_COLUMN;
  if (LINES > MAX_LINE)
    LINES = MAX_LINE;
}

static struct w3m_term_info {
  char *term;
  char *title_str;
} w3m_term_info_list[] = {
    {W3M_TERM_INFO("xterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("kterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("rxvt", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("Eterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("mlterm", XTERM_TITLE, (NEED_XTERM_ON | NEED_XTERM_OFF))},
    {W3M_TERM_INFO("screen", SCREEN_TITLE, 0)},
    {W3M_TERM_INFO(NULL, NULL, 0)}};
#undef W3M_TERM_INFO

void term_reset() {
  term_puts(termcap_str_orig_pair());
  term_puts(termcap_str_exit_attribute_mode());
  if (!Do_not_use_ti_te) {
    term_puts(termcap_str_te());
  }
  term_puts(termcap_str_reset()); /* reset terminal */
  tty_flush();
  tty_close();
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval) {
  term_reset();
  w3m_exit(rval);
}

MySignalHandler reset_error_exit(SIGNAL_ARG) {
  reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler reset_exit(SIGNAL_ARG) {
  reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler error_dump(SIGNAL_ARG) {
  mySignal(SIGIOT, SIG_DFL);
  term_reset();
  abort();
}

bool fmInitialized = false;

void set_int() {
  mySignal(SIGHUP, reset_exit);
  mySignal(SIGINT, reset_exit);
  mySignal(SIGQUIT, reset_exit);
  mySignal(SIGTERM, reset_exit);
  mySignal(SIGILL, error_dump);
  mySignal(SIGIOT, error_dump);
  mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
  mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
       /* mySignal(SIGSEGV, error_dump); */
}

static void term_setgraphchar() {
  for (int c = 0; c < 96; c++) {
    gcmap[c] = (char)(c + ' ');
  }

  auto acs = termcap_str_acs_chars();
  if (acs) {
    int n = strlen(acs);
    for (int i = 0; i < n - 1; i += 2) {
      auto c = (unsigned)acs[i] - ' ';
      if (c >= 0 && c < 96) {
        gcmap[c] = acs[i + 1];
      }
    }
  }
}

void getTCstr() {

  auto ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
  if (ent == NULL) {
    fprintf(stderr, "TERM is not set\n");
    reset_error_exit(SIGNAL_ARGLIST);
  }

  if (!termcap_interface_load(ent)) {
    /* Can't find termcap entry */
    fprintf(stderr, "Can't find termcap entry %s\n", ent);
    reset_error_exit(SIGNAL_ARGLIST);
  }

  LINES = COLS = 0;
  term_setlinescols();
  term_setgraphchar();
}

/*
 * Screen initialize
 */
static char *title_str = NULL;
void term_title(const char *s) {
  if (!fmInitialized)
    return;
  if (title_str != NULL) {
    tty_printf(title_str, s);
  }
}

int term_init() {
  tty_open();

  if (displayTitleTerm != NULL) {
    struct w3m_term_info *p;
    for (p = w3m_term_info_list; p->term != NULL; p++) {
      if (!strncmp(displayTitleTerm, p->term, strlen(p->term))) {
        title_str = p->title_str;
        break;
      }
    }
  }

  set_int();
  getTCstr();
  auto ti = termcap_str_enter_ca_mode();
  if (ti && !Do_not_use_ti_te) {
    term_puts(ti);
  }
  scr_setup(LINES, COLS);
  return 0;
}

bool term_graph_ok() {
  if (UseGraphicChar != GRAPHIC_CHAR_DEC)
    return false;
  return termcap_graph_ok();
}

void term_clear() { term_puts(termcap_str_clear()); }

void term_move(int line, int column) {
  term_puts(tgoto(termcap_str_cursor_mv(), column, line));
}

void term_bell() { tty_putc(7); }

enum {
  RF_NEED_TO_MOVE,
  RF_CR_OK,
  RF_NONEED_TO_MOVE,
};

static char *color_seq(int colmode) {
  static char seqbuf[32];
  sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + 30);
  return seqbuf;
}

#define graphchar(c)                                                           \
  (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c))

void term_refresh() {
  int line, col, pcol;

  auto scr = scr_get();

  int pline = scr->CurLine;
  int moved = RF_NEED_TO_MOVE;
  char *pc;
  l_prop *pr;
  l_prop mode = 0;
  l_prop color = COL_FTERM;
  short *dirty;

  for (line = 0; line <= LASTLINE; line++) {
    dirty = &scr->ScreenImage[line]->isdirty;
    if (*dirty & L_DIRTY) {
      *dirty &= ~L_DIRTY;
      pc = scr->ScreenImage[line]->lineimage;
      pr = scr->ScreenImage[line]->lineprop;
      for (col = 0; col < COLS && !(pr[col] & S_EOL); col++) {
        if (*dirty & L_NEED_CE && col >= scr->ScreenImage[line]->eol) {
          if (scr_need_redraw(pc[col], pr[col], SPACE, 0))
            break;
        } else {
          if (pr[col] & S_DIRTY)
            break;
        }
      }
      if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
        pcol = scr->ScreenImage[line]->eol;
        if (pcol >= COLS) {
          *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
          pcol = col;
        }
      } else {
        pcol = col;
      }
      if (line < LINES - 2 && pline == line - 1 && pcol == 0) {
        switch (moved) {
        case RF_NEED_TO_MOVE:
          term_move(line, 0);
          moved = RF_CR_OK;
          break;
        case RF_CR_OK:
          tty_putc('\n');
          tty_putc('\r');
          break;
        case RF_NONEED_TO_MOVE:
          moved = RF_CR_OK;
          break;
        }
      } else {
        term_move(line, pcol);
        moved = RF_CR_OK;
      }
      if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
        term_puts(termcap_str_clr_eol());
        if (col != pcol)
          term_move(line, col);
      }
      pline = line;
      pcol = col;
      for (; col < COLS; col++) {
        if (pr[col] & S_EOL)
          break;

          /*
           * some terminal emulators do linefeed when a
           * character is put on COLS-th column. this behavior
           * is different from one of vt100, but such terminal
           * emulators are used as vt100-compatible
           * emulators. This behaviour causes scroll when a
           * character is drawn on (COLS-1,LINES-1) point.  To
           * avoid the scroll, I prohibit to draw character on
           * (COLS-1,LINES-1).
           */
#if !defined(USE_BG_COLOR)
        if (line == LINES - 1 && col == COLS - 1)
          break;
#endif /* !defined(USE_BG_COLOR)  */
        if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) ||
            (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) ||
            (!(pr[col] & S_BOLD) && (mode & S_BOLD)) ||
            (!(pr[col] & S_COLORED) && (mode & S_COLORED)) ||
            (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
          if ((mode & S_COLORED))
            term_puts(termcap_str_orig_pair());
          if (mode & S_GRAPHICS)
            term_puts(termcap_str_exit_alt_charset_mode());
          term_puts(termcap_str_exit_attribute_mode());
          mode &= ~M_MEND;
        }
        if ((*dirty & L_NEED_CE && col >= scr->ScreenImage[line]->eol)
                ? scr_need_redraw(pc[col], pr[col], SPACE, 0)
                : (pr[col] & S_DIRTY)) {
          if (pcol == col - 1)
            term_puts(termcap_str_cursor_right());
          else if (pcol != col)
            term_move(line, col);

          if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
            term_puts(termcap_str_enter_standout_mode());
            mode |= S_STANDOUT;
          }
          if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
            term_puts(termcap_str_enter_underline_mode());
            mode |= S_UNDERLINE;
          }
          if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
            term_puts(termcap_str_enter_bold_mode());
            mode |= S_BOLD;
          }
          if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
            color = (pr[col] & COL_FCOLOR);
            mode = ((mode & ~COL_FCOLOR) | color);
            term_puts(color_seq(color));
          }
          if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
            if (!scr->graph_enabled) {
              scr->graph_enabled = 1;
              term_puts(termcap_str_ena_acs());
            }
            term_puts(termcap_str_enter_alt_charset_mode());
            mode |= S_GRAPHICS;
          }
          tty_putc((pr[col] & S_GRAPHICS) ? graphchar(pc[col]) : pc[col]);
          pcol = col + 1;
        }
      }
      if (col == COLS)
        moved = RF_NEED_TO_MOVE;
      for (; col < COLS && !(pr[col] & S_EOL); col++)
        pr[col] |= S_EOL;
    }
    *dirty &= ~(L_NEED_CE | L_CLRTOEOL);
    if (mode & M_MEND) {
      if (mode & (S_COLORED))
        term_puts(termcap_str_orig_pair());
      if (mode & S_GRAPHICS) {
        term_puts(termcap_str_exit_alt_charset_mode());
      }
      term_puts(termcap_str_exit_attribute_mode());
      mode &= ~M_MEND;
    }
  }
  term_move(scr->CurLine, scr->CurColumn);
  tty_flush();
}

void term_fmInit() {
  if (!fmInitialized) {
    term_init();
    tty_raw();
    tty_noecho();
  }
  fmInitialized = TRUE;
}

bool term_is_initialized() { return fmInitialized; }

void term_fmTerm() {
  if (fmInitialized) {
    scr_move(LASTLINE, 0);
    scr_clrtoeolx();
    term_refresh();
    term_reset();
    fmInitialized = FALSE;
  }
}

void term_raw() {
  if (fmInitialized)
    tty_raw();
}

void term_cbreak() {
  if (fmInitialized)
    tty_cbreak();
}

void term_message(const char *msg) {
  if (fmInitialized) {
    scr_message(msg, 0, 0);
    term_refresh();
  } else {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
}

static void reset_signals(void) {
#ifdef SIGHUP
  mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
  mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
  mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
  mySignal(SIGTERM, SIG_DFL); /* terminate process */
  mySignal(SIGILL, SIG_DFL);  /* create core image */
  mySignal(SIGIOT, SIG_DFL);  /* create core image */
  mySignal(SIGFPE, SIG_DFL);  /* create core image */
#ifdef SIGBUS
  mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif                       /* SIGBUS */
  mySignal(SIGCHLD, SIG_IGN);
  mySignal(SIGPIPE, SIG_IGN);
}

static void close_all_fds_except(int i, int f) {
  switch (i) { /* fall through */
  case 0:
    dup2(open(DEV_NULL_PATH, O_RDONLY), 0);
  case 1:
    dup2(open(DEV_NULL_PATH, O_WRONLY), 1);
  case 2:
    dup2(open(DEV_NULL_PATH, O_WRONLY), 2);
  }
  /* close all other file descriptors (socket, ...) */
  for (i = 3; i < FOPEN_MAX; i++) {
    if (i != f)
      close(i);
  }
}

void setup_child(int child, int i, int f) {
  reset_signals();
  mySignal(SIGINT, SIG_IGN);
  if (!child)
    SETPGRP();
  /*
   * I don't know why but close_tty() sometimes interrupts loadGeneralFile() in
   * loadImage() and corrupt image data can be cached in ~/.w3m.
   */
  close_all_fds_except(i, f);
  QuietMessage = TRUE;
  fmInitialized = FALSE;
  TrapSignal = FALSE;
}

Str term_inputpwd() {
  Str pwd = nullptr;
  if (fmInitialized) {
    tty_raw();
    pwd = Strnew_charp(inputLine("Password: ", NULL, IN_PASSWORD));
    pwd = Str_conv_to_system(pwd);
    tty_cbreak();
  } else {
    pwd = Strnew_charp((char *)getpass("Password: "));
  }
  return pwd;
}

void term_input(const char *msg) {
  if (fmInitialized) {
    /* FIXME: gettextize? */
    inputChar(msg);
  }
}

char *term_inputAnswer(char *prompt) {
  char *ans;

  if (QuietMessage)
    return "n";
  if (fmInitialized) {
    tty_raw();
    ans = inputChar(prompt);
  } else {
    printf("%s", prompt);
    fflush(stdout);
    ans = Strfgets(stdin)->ptr;
  }
  return ans;
}

void term_showProgress(int64_t *linelen, int64_t *trbyte,
                       int64_t current_content_length) {
  int i, j, rate, duration, eta, pos;
  static time_t last_time, start_time;
  time_t cur_time;
  Str messages;
  char *fmtrbyte, *fmrate;

  if (!fmInitialized)
    return;

  if (*linelen < 1024)
    return;
  if (current_content_length > 0) {
    double ratio;
    cur_time = time(0);
    if (*trbyte == 0) {
      scr_move(LASTLINE, 0);
      scr_clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    scr_move(LASTLINE, 0);
    ratio = 100.0 * (*trbyte) / current_content_length;
    fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
    duration = cur_time - start_time;
    if (duration) {
      rate = *trbyte / duration;
      fmrate = convert_size(rate, 1);
      eta = rate ? (current_content_length - *trbyte) / rate : -1;
      messages = Sprintf("%11s %3.0f%% "
                         "%7s/s "
                         "eta %02d:%02d:%02d     ",
                         fmtrbyte, ratio, fmrate, eta / (60 * 60),
                         (eta / 60) % 60, eta % 60);
    } else {
      messages =
          Sprintf("%11s %3.0f%%                          ", fmtrbyte, ratio);
    }
    scr_addstr(messages->ptr);
    pos = 42;
    i = pos + (COLS - pos - 1) * (*trbyte) / current_content_length;
    scr_move(LASTLINE, pos);
    scr_standout();
    scr_addch(' ');
    for (j = pos + 1; j <= i; j++)
      scr_addch('|');
    scr_standend();
    /* no_clrtoeol(); */
    term_refresh();
  } else {
    cur_time = time(0);
    if (*trbyte == 0) {
      scr_move(LASTLINE, 0);
      scr_clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    scr_move(LASTLINE, 0);
    fmtrbyte = convert_size(*trbyte, 1);
    duration = cur_time - start_time;
    if (duration) {
      fmrate = convert_size(*trbyte / duration, 1);
      messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
    } else {
      messages = Sprintf("%7s loaded", fmtrbyte);
    }
    scr_message(messages->ptr, 0, 0);
    term_refresh();
  }
}

bool term_inputAuth(const char *realm, bool proxy, Str *uname, Str *pwd) {
  if (fmInitialized) {
    char *pp;
    tty_raw();
    /* FIXME: gettextize? */
    if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr, NULL)) == NULL)
      return false;
    *uname = Str_conv_to_system(Strnew_charp(pp));
    if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
                        IN_PASSWORD)) == NULL) {
      *uname = NULL;
      return false;
    }
    *pwd = Str_conv_to_system(Strnew_charp(pp));
    tty_cbreak();
  } else {
    /*
     * If post file is specified as '-', stdin is closed at this
     * point.
     * In this case, w3m cannot read username from stdin.
     * So exit with error message.
     * (This is same behavior as lwp-request.)
     */
    if (feof(stdin) || ferror(stdin)) {
      /* FIXME: gettextize? */
      fprintf(stderr, "w3m: Authorization required for %s\n", realm);
      exit(1);
    }

    /* FIXME: gettextize? */
    printf(proxy ? "Proxy Username for %s: " : "Username for %s: ", realm);
    fflush(stdout);
    *uname = Strfgets(stdin);
    Strchop(*uname);
#ifdef HAVE_GETPASSPHRASE
    *pwd = Strnew_charp(
        (char *)getpassphrase(proxy ? "Proxy Password: " : "Password: "));
#else
    *pwd = Strnew_charp(
        (char *)getpass(proxy ? "Proxy Password: " : "Password: "));
#endif
  }

  return true;
}

static GeneralList *message_list = NULL;

void term_err_message(const char *s) {
  if (!message_list)
    message_list = newGeneralList();
  if (message_list->nitem >= LINES)
    popValue(message_list);
  pushValue(message_list, allocStr(s, -1));
}

const char *term_message_to_html() {
  if (!message_list) {
    return "<tr><td>(no message recorded)</td></tr>\n";
  }

  auto tmp = Strnew();
  for (auto p = message_list->last; p; p = p->prev)
    Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                   "</pre></td></tr>\n", NULL);
  return tmp->ptr;
}

void disp_err_message(char *s, int redraw_current) {
  term_err_message(s);
  disp_message(s, redraw_current);
}

void disp_message_nsec(char *s, int redraw_current, int sec, int purge,
                       int mouse) {
  if (QuietMessage)
    return;

  if (term_is_initialized()) {
    if (CurrentTab != NULL && Currentbuf != NULL) {
      scr_message(s, Currentbuf->cursorX + Currentbuf->rootX,
                  Currentbuf->cursorY + Currentbuf->rootY);
    } else {
      scr_message(s, LASTLINE, 0);
    }
    term_refresh();

    // nsec
    tty_sleep_till_anykey(sec, purge);
    if (CurrentTab != NULL && Currentbuf != NULL && redraw_current) {
      displayBuffer(Currentbuf, B_NORMAL);
    }
  } else {
    fprintf(stderr, "%s\n", conv_to_system(s));
  }
}

void disp_message(char *s, int redraw_current) {
  disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

static char *delayed_msg = NULL;
void set_delayed_message(char *s) { delayed_msg = allocStr(s, -1); }

void term_show_delayed_message() {
  if (delayed_msg != NULL) {
    disp_message(delayed_msg, FALSE);
    delayed_msg = NULL;
    term_refresh();
  }
}
