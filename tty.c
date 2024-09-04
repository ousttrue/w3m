#include "tty.h"
#include "config.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/select.h>
#include <stdarg.h>
#include "ctrlcode.h"

#define DEV_TTY_PATH "/dev/tty"

static int g_tty = -1;
static FILE *g_ttyf = NULL;

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

static TerminalMode d_ioval;

void tty_open() {
  char *ttyn;
  if (isatty(0)) /* stdin */
    ttyn = ttyname(0);
  else
    ttyn = DEV_TTY_PATH;

  g_tty = open(ttyn, O_RDWR);
  if (g_tty < 0) {
    /* use stderr instead of stdin... is it OK???? */
    g_tty = 2;
  }
  g_ttyf = fdopen(g_tty, "w");
  TerminalGet(g_tty, &d_ioval);
}

void tty_close() {
  TerminalSet(g_tty, &d_ioval);
  if (g_tty > 2)
    close(g_tty);
}

void flush_tty() {
  if (g_ttyf)
    fflush(g_ttyf);
}

static void ttymode_set(int mode, int imode) {
  TerminalMode ioval;

  TerminalGet(g_tty, &ioval);
  MODEFLAG(ioval) |= mode;
#ifndef HAVE_SGTTY_H
  IMODEFLAG(ioval) |= imode;
#endif /* not HAVE_SGTTY_H */

  while (TerminalSet(g_tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred while set %x: errno=%d\n", mode, errno);
    // reset_error_exit(SIGNAL_ARGLIST);
    tty_close();
  }
}

#ifndef HAVE_SGTTY_H
static void set_cc(int spec, int val) {
  TerminalMode ioval;

  TerminalGet(g_tty, &ioval);
  ioval.c_cc[spec] = val;
  while (TerminalSet(g_tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred: errno=%d\n", errno);
    // reset_error_exit(SIGNAL_ARGLIST);
    tty_close();
  }
}
#endif /* not HAVE_SGTTY_H */

static void ttymode_reset(int mode, int imode) {
  TerminalMode ioval;

  TerminalGet(g_tty, &ioval);
  MODEFLAG(ioval) &= ~mode;
#ifndef HAVE_SGTTY_H
  IMODEFLAG(ioval) &= ~imode;
#endif /* not HAVE_SGTTY_H */

  while (TerminalSet(g_tty, &ioval) == -1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    printf("Error occurred while reset %x: errno=%d\n", mode, errno);
    // reset_error_exit(SIGNAL_ARGLIST);
    tty_close();
  }
}

void tty_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(g_ttyf, fmt, ap);
}

int term_putc(char c) {
  putc(c, g_ttyf);
#ifdef SCREEN_DEBUG
  flush_tty();
#endif /* SCREEN_DEBUG */
  return 0;
}

char *ttyname_tty(void) { return ttyname(g_tty); }

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

char getch() {
  char c;

  while (read(g_tty, &c, 1) < (int)1) {
    if (errno == EINTR || errno == EAGAIN)
      continue;
    /* error happend on read(2) */
    // quitfm();
    tty_close();
    break; /* unreachable */
  }
  return c;
}

static void skip_escseq(void) {
  int c = getch();
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

  TerminalGet(g_tty, &ioval);
  term_raw();

  tim.tv_sec = sec;
  tim.tv_usec = 0;

  FD_ZERO(&rfd);
  FD_SET(g_tty, &rfd);

  ret = select(g_tty + 1, &rfd, 0, 0, &tim);
  if (ret > 0 && purge) {
    c = getch();
    if (c == ESC_CODE)
      skip_escseq();
  }
  er = TerminalSet(g_tty, &ioval);
  if (er == -1) {
    printf("Error occurred: errno=%d\n", errno);
    // reset_error_exit(SIGNAL_ARGLIST);
    tty_close();
  }
  return ret;
}
