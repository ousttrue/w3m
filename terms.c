/*
 * An original curses library for EUC-kanji by Akinori ITO,     December 1989
 * revised by Akinori ITO, January 1995
 */
#include "terms.h"
#include "tty.h"
#include "scr.h"
#include "termseq/termcap_entry.h"
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
  auto t = termcap_get();
  term_puts(t->T_op); /* turn off */
  term_puts(t->T_me);
  if (!Do_not_use_ti_te) {
    if (t->T_te && *t->T_te)
      term_puts(t->T_te);
    else
      term_puts(t->T_cl);
  }
  term_puts(t->T_se); /* reset terminal */
  tty_flush();
  tty_close();
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval) {
  term_reset();
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
  term_reset();
  abort();
  SIGNAL_RETURN;
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

  auto t = termcap_get();
  if (!t->T_ac) {
    return;
  }

  int n = strlen(t->T_ac);
  for (int i = 0; i < n - 1; i += 2) {
    auto c = (unsigned)t->T_ac[i] - ' ';
    if (c >= 0 && c < 96)
      gcmap[c] = t->T_ac[i + 1];
  }
}

void getTCstr() {

  auto ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
  if (ent == NULL) {
    fprintf(stderr, "TERM is not set\n");
    reset_error_exit(SIGNAL_ARGLIST);
  }

  if(!termcap_load(ent)){
    /* Can't find termcap entry */
    fprintf(stderr, "Can't find termcap entry %s\n", ent);
    reset_error_exit(SIGNAL_ARGLIST);
  }

  LINES = COLS = 0;
  termcap_setlinescols();
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
  auto t = termcap_get();
  if (t->T_ti && !Do_not_use_ti_te)
    term_puts(t->T_ti);
  scr_setup(LINES, COLS);
  return 0;
}

bool term_graph_ok() {
  if (UseGraphicChar != GRAPHIC_CHAR_DEC)
    return false;
  auto t = termcap_get();
  return t->T_as[0] != 0 && t->T_ae[0] != 0 && t->T_ac[0] != 0;
}

void term_clear() { term_puts(termcap_get()->T_cl); }

void term_move(int line, int column) {
  term_puts(tgoto(termcap_get()->T_cm, column, line));
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

  auto t = termcap_get();
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
        term_puts(t->T_ce);
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
            term_puts(t->T_op);
          if (mode & S_GRAPHICS)
            term_puts(t->T_ae);
          term_puts(t->T_me);
          mode &= ~M_MEND;
        }
        if ((*dirty & L_NEED_CE && col >= scr->ScreenImage[line]->eol)
                ? scr_need_redraw(pc[col], pr[col], SPACE, 0)
                : (pr[col] & S_DIRTY)) {
          if (pcol == col - 1)
            term_puts(t->T_nd);
          else if (pcol != col)
            term_move(line, col);

          if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
            term_puts(t->T_so);
            mode |= S_STANDOUT;
          }
          if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
            term_puts(t->T_us);
            mode |= S_UNDERLINE;
          }
          if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
            term_puts(t->T_md);
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
              term_puts(t->T_eA);
            }
            term_puts(t->T_as);
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
        term_puts(t->T_op);
      if (mode & S_GRAPHICS) {
        term_puts(t->T_ae);
      }
      term_puts(t->T_me);
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

void term_fmTerm() {
  if (fmInitialized) {
    scr_move(LASTLINE, 0);
    scr_clrtoeolx();
    term_refresh();
    term_reset();
    fmInitialized = FALSE;
  }
}

