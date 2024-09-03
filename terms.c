/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "scr.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#include <string.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/ioctl.h>

static char *title_str = NULL;

static int tty;

#include "fm.h"
#include "myctype.h"

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler reset_error_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

#ifdef HAVE_TERMIO_H
#include <termio.h>
typedef struct termio TerminalMode;
#define TerminalSet(fd, x) ioctl(fd, TCSETA, x)
#define TerminalGet(fd, x) ioctl(fd, TCGETA, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIO_H */

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#include <unistd.h>
typedef struct termios TerminalMode;
#define TerminalSet(fd, x) tcsetattr(fd, TCSANOW, x)
#define TerminalGet(fd, x) tcgetattr(fd, x)
#define MODEFLAG(d) ((d).c_lflag)
#define IMODEFLAG(d) ((d).c_iflag)
#endif /* HAVE_TERMIOS_H */

#ifdef HAVE_SGTTY_H
#include <sgtty.h>
typedef struct sgttyb TerminalMode;
#define TerminalSet(fd, x) ioctl(fd, TIOCSETP, x)
#define TerminalGet(fd, x) ioctl(fd, TIOCGETP, x)
#define MODEFLAG(d) ((d).sg_flags)
#endif /* HAVE_SGTTY_H */

#define MAX_LINE 200
#define MAX_COLUMN 400

static TerminalMode d_ioval;
static int tty = -1;
static FILE *ttyf = NULL;

static char bp[1024], funcstr[256];

char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc, *T_so,
    *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me, *T_ti, *T_te,
    *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

int LINES, COLS;


char gcmap[96];

int term_putc(char c) {
  putc(c, ttyf);
#ifdef SCREEN_DEBUG
  flush_tty();
#endif /* SCREEN_DEBUG */
  return 0;
}

void term_puts(const char *s) { tputs(s, 1, term_putc); }

#define W3M_TERM_INFO(name, title, mouse) name, title

static char XTERM_TITLE[] = "\033]0;w3m: %s\007";
static char SCREEN_TITLE[] = "\033k%s\033\134";

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

int set_tty(void) {
  char *ttyn;

  if (isatty(0)) /* stdin */
    ttyn = ttyname(0);
  else
    ttyn = DEV_TTY_PATH;
  tty = open(ttyn, O_RDWR);
  if (tty < 0) {
    /* use stderr instead of stdin... is it OK???? */
    tty = 2;
  }
  ttyf = fdopen(tty, "w");
  TerminalGet(tty, &d_ioval);
  if (displayTitleTerm != NULL) {
    struct w3m_term_info *p;
    for (p = w3m_term_info_list; p->term != NULL; p++) {
      if (!strncmp(displayTitleTerm, p->term, strlen(p->term))) {
        title_str = p->title_str;
        break;
      }
    }
  }
  return 0;
}

static void ttymode_set(int mode, int imode) {
  TerminalMode ioval;

  TerminalGet(tty, &ioval);
  MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
  IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

  while (TerminalSet(tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred while set %x: errno=%d\n", mode, errno);
    reset_error_exit(SIGNAL_ARGLIST);
  }
}

static void ttymode_reset(int mode, int imode) {
  TerminalMode ioval;

  TerminalGet(tty, &ioval);
  MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
  IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

  while (TerminalSet(tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred while reset %x: errno=%d\n", mode, errno);
    reset_error_exit(SIGNAL_ARGLIST);
  }
}

#ifndef HAVE_SGTTY_H
static void set_cc(int spec, int val) {
  TerminalMode ioval;

  TerminalGet(tty, &ioval);
  ioval.c_cc[spec] = val;
  while (TerminalSet(tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred: errno=%d\n", errno);
    reset_error_exit(SIGNAL_ARGLIST);
  }
}
#endif /* not HAVE_SGTTY_H */

void close_tty(void) {
  if (tty > 2)
    close(tty);
}

char *ttyname_tty(void) { return ttyname(tty); }

void reset_tty(void) {
  term_puts(T_op); /* turn off */
  term_puts(T_me);
  if (!Do_not_use_ti_te) {
    if (T_te && *T_te)
      term_puts(T_te);
    else
      term_puts(T_cl);
  }
  term_puts(T_se); /* reset terminal */
  flush_tty();
  TerminalSet(tty, &d_ioval);
  if (tty != 2)
    close_tty();
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval) {
  reset_tty();
  w3m_exit(rval);
  SIGNAL_RETURN;
}

MySignalHandler reset_error_exit(SIGNAL_ARG) {
  reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler reset_exit(SIGNAL_ARG) {
  reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler error_dump(SIGNAL_ARG) {
  mySignal(SIGIOT, SIG_DFL);
  reset_tty();
  abort();
  SIGNAL_RETURN;
}

void set_int(void) {
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

static void setgraphchar(void) {
  int c, i, n;

  for (c = 0; c < 96; c++)
    gcmap[c] = (char)(c + ' ');

  if (!T_ac)
    return;

  n = strlen(T_ac);
  for (i = 0; i < n - 1; i += 2) {
    c = (unsigned)T_ac[i] - ' ';
    if (c >= 0 && c < 96)
      gcmap[c] = T_ac[i + 1];
  }
}

#define GETSTR(v, s)                                                           \
  {                                                                            \
    v = pt;                                                                    \
    suc = tgetstr(s, &pt);                                                     \
    if (!suc)                                                                  \
      v = "";                                                                  \
    else                                                                       \
      v = allocStr(suc, -1);                                                   \
  }

void getTCstr(void) {
  char *ent;
  char *suc;
  char *pt = funcstr;
  int r;

  ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
  if (ent == NULL) {
    fprintf(stderr, "TERM is not set\n");
    reset_error_exit(SIGNAL_ARGLIST);
  }

  r = tgetent(bp, ent);
  if (r != 1) {
    /* Can't find termcap entry */
    fprintf(stderr, "Can't find termcap entry %s\n", ent);
    reset_error_exit(SIGNAL_ARGLIST);
  }

  GETSTR(T_ce, "ce"); /* clear to the end of line */
  GETSTR(T_cd, "cd"); /* clear to the end of display */
  GETSTR(T_kr, "nd"); /* cursor right */
  if (suc == NULL)
    GETSTR(T_kr, "kr");
  if (tgetflag("bs"))
    T_kl = "\b"; /* cursor left */
  else {
    GETSTR(T_kl, "le");
    if (suc == NULL)
      GETSTR(T_kl, "kb");
    if (suc == NULL)
      GETSTR(T_kl, "kl");
  }
  GETSTR(T_cr, "cr"); /* carriage return */
  GETSTR(T_ta, "ta"); /* tab */
  GETSTR(T_sc, "sc"); /* save cursor */
  GETSTR(T_rc, "rc"); /* restore cursor */
  GETSTR(T_so, "so"); /* standout mode */
  GETSTR(T_se, "se"); /* standout mode end */
  GETSTR(T_us, "us"); /* underline mode */
  GETSTR(T_ue, "ue"); /* underline mode end */
  GETSTR(T_md, "md"); /* bold mode */
  GETSTR(T_me, "me"); /* bold mode end */
  GETSTR(T_cl, "cl"); /* clear screen */
  GETSTR(T_cm, "cm"); /* cursor move */
  GETSTR(T_al, "al"); /* append line */
  GETSTR(T_sr, "sr"); /* scroll reverse */
  GETSTR(T_ti, "ti"); /* terminal init */
  GETSTR(T_te, "te"); /* terminal end */
  GETSTR(T_nd, "nd"); /* move right one space */
  GETSTR(T_eA, "eA"); /* enable alternative charset */
  GETSTR(T_as, "as"); /* alternative (graphic) charset start */
  GETSTR(T_ae, "ae"); /* alternative (graphic) charset end */
  GETSTR(T_ac, "ac"); /* graphics charset pairs */
  GETSTR(T_op, "op"); /* set default color pair to its original value */
#if defined(CYGWIN) && CYGWIN < 1
  /* for TERM=pcansi on MS-DOS prompt. */
  T_eA = "";
  T_as = "";
  T_ae = "";
  T_ac = "";
#endif /* CYGWIN */

  LINES = COLS = 0;
  setlinescols();
  setgraphchar();
}

void setlinescols(void) {
  char *p;
  int i;
  if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    LINES = i;
  if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    COLS = i;
  if (LINES <= 0)
    LINES = tgetnum("li"); /* number of line */
  if (COLS <= 0)
    COLS = tgetnum("co"); /* number of column */
  if (COLS > MAX_COLUMN)
    COLS = MAX_COLUMN;
  if (LINES > MAX_LINE)
    LINES = MAX_LINE;
}

/*
 * Screen initialize
 */
int initscr(void) {
  if (set_tty() < 0)
    return -1;
  set_int();
  getTCstr();
  if (T_ti && !Do_not_use_ti_te)
    term_puts(T_ti);
  setupscreen(LINES, COLS);
  return 0;
}

int graph_ok(void) {
  if (UseGraphicChar != GRAPHIC_CHAR_DEC)
    return 0;
  return T_as[0] != 0 && T_ae[0] != 0 && T_ac[0] != 0;
}

// void setfcolor(int color) {
//   CurrentMode &= ~COL_FCOLOR;
//   if ((color & 0xf) <= 7)
//     CurrentMode |= (((color & 7) | 8) << 8);
// }

void term_clear() { term_puts(T_cl); }

void term_move(int line, int column) { term_puts(tgoto(T_cm, column, line)); }

void crmode(void)
#ifndef HAVE_SGTTY_H
{
  ttymode_reset(ICANON, IXON);
  ttymode_set(ISIG, 0);
#ifdef HAVE_TERMIOS_H
  set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
  set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
  ttymode_set(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void nocrmode(void)
#ifndef HAVE_SGTTY_H
{
  ttymode_set(ICANON, 0);
#ifdef HAVE_TERMIOS_H
  set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
  set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
  ttymode_reset(CBREAK, 0);
}
#endif /* HAVE_SGTTY_H */

void term_echo(void) { ttymode_set(ECHO, 0); }

void term_noecho(void) { ttymode_reset(ECHO, 0); }

void term_raw(void)
#ifndef HAVE_SGTTY_H
#ifdef IEXTEN
#define TTY_MODE ISIG | ICANON | ECHO | IEXTEN
#else /* not IEXTEN */
#define TTY_MODE ISIG | ICANON | ECHO
#endif /* not IEXTEN */
{
  ttymode_reset(TTY_MODE, IXON | IXOFF);
#ifdef HAVE_TERMIOS_H
  set_cc(VMIN, 1);
#else  /* not HAVE_TERMIOS_H */
  set_cc(VEOF, 1);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
  ttymode_set(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cooked(void)
#ifndef HAVE_SGTTY_H
{
  ttymode_set(TTY_MODE, 0);
#ifdef HAVE_TERMIOS_H
  set_cc(VMIN, 4);
#else  /* not HAVE_TERMIOS_H */
  set_cc(VEOF, 4);
#endif /* not HAVE_TERMIOS_H */
}
#else  /* HAVE_SGTTY_H */
{
  ttymode_reset(RAW, 0);
}
#endif /* HAVE_SGTTY_H */

void term_cbreak(void) {
  term_cooked();
  term_noecho();
}

void term_title(const char *s) {
  if (!fmInitialized)
    return;
  if (title_str != NULL) {
    fprintf(ttyf, title_str, s);
  }
}

char getch() {
  char c;

  while (read(tty, &c, 1) < (int)1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    /* error happend on read(2) */
    quitfm();
    break; /* unreachable */
  }
  return c;
}

void bell(void) { term_putc(7); }

static void skip_escseq(void) {
  int c;

  c = getch();
  if (c == '[' || c == 'O') {
    c = getch();
    while (IS_DIGIT(c))
      c = getch();
  }
}

int sleep_till_anykey(int sec, int purge) {
  fd_set rfd;
  struct timeval tim;
  int er, c, ret;
  TerminalMode ioval;

  TerminalGet(tty, &ioval);
  term_raw();

  tim.tv_sec = sec;
  tim.tv_usec = 0;

  FD_ZERO(&rfd);
  FD_SET(tty, &rfd);

  ret = select(tty + 1, &rfd, 0, 0, &tim);
  if (ret > 0 && purge) {
    c = getch();
    if (c == ESC_CODE)
      skip_escseq();
  }
  er = TerminalSet(tty, &ioval);
  if (er == -1) {
    printf("Error occurred: errno=%d\n", errno);
    reset_error_exit(SIGNAL_ARGLIST);
  }
  return ret;
}

void flush_tty() {
  if (ttyf)
    fflush(ttyf);
}
