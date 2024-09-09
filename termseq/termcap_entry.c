#include "termcap_entry.h"
#include "termcap_getentry.h"
#include <stdlib.h>

#define MAX_LINE 200
#define MAX_COLUMN 400

int LINES;
int COLS;

static TermCap s_termcap;
TermCap *termcap_get() { return &s_termcap; };

bool termcap_load(const char *ent) {
  if (tgetent(s_termcap.bp, ent) != 1) {
    return false;
  }

  auto t = &s_termcap;
  auto pt = t->funcstr;
  t->T_ce = tgetstr("ce", &pt); /* clear to the end of line */
  t->T_cd = tgetstr("cd", &pt); /* clear to the end of display */
  t->T_kr = tgetstr("nd", &pt); /* cursor right */
  if (!t->T_kr) {
    t->T_kr = tgetstr("kr", &pt);
  }
  if (tgetflag("bs")) {
    t->T_kl = "\b"; /* cursor left */
  } else {
    t->T_kl = tgetstr("le", &pt);
    if (!t->T_kl) {
      t->T_kl = tgetstr("kb", &pt);
    }
    if (!t->T_kl) {
      t->T_kl = tgetstr("kl", &pt);
    }
  }
  t->T_cr = tgetstr("cr", &pt); /* carriage return */
  t->T_ta = tgetstr("ta", &pt); /* tab */
  t->T_sc = tgetstr("sc", &pt); /* save cursor */
  t->T_rc = tgetstr("rc", &pt); /* restore cursor */
  t->T_so = tgetstr("so", &pt); /* standout mode */
  t->T_se = tgetstr("se", &pt); /* standout mode end */
  t->T_us = tgetstr("us", &pt); /* underline mode */
  t->T_ue = tgetstr("ue", &pt); /* underline mode end */
  t->T_md = tgetstr("md", &pt); /* bold mode */
  t->T_me = tgetstr("me", &pt); /* bold mode end */
  t->T_cl = tgetstr("cl", &pt); /* clear screen */
  t->T_cm = tgetstr("cm", &pt); /* cursor move */
  t->T_al = tgetstr("al", &pt); /* append line */
  t->T_sr = tgetstr("sr", &pt); /* scroll reverse */
  t->T_ti = tgetstr("ti", &pt); /* terminal init */
  t->T_te = tgetstr("te", &pt); /* terminal end */
  t->T_nd = tgetstr("nd", &pt); /* move right one space */
  t->T_eA = tgetstr("eA", &pt); /* enable alternative charset */
  t->T_as = tgetstr("as", &pt); /* alternative (graphic) charset start */
  t->T_ae = tgetstr("ae", &pt); /* alternative (graphic) charset end */
  t->T_ac = tgetstr("ac", &pt); /* graphics charset pairs */
  t->T_op =
      tgetstr("op", &pt);   /* set default color pair to its original value */
  t->LINES = tgetnum("li"); /* number of line */
  t->COLS = tgetnum("co");  /* number of column */
  return true;
}

void termcap_setlinescols() {
  char *p;
  int i;
  if (LINES <= 0 && (p = getenv("LINES")) != NULL && (i = atoi(p)) >= 0)
    LINES = i;
  if (COLS <= 0 && (p = getenv("COLUMNS")) != NULL && (i = atoi(p)) >= 0)
    COLS = i;
  if (LINES <= 0)
    LINES = s_termcap.LINES; /* number of line */
  if (COLS <= 0)
    COLS = s_termcap.COLS; /* number of column */
  if (COLS > MAX_COLUMN)
    COLS = MAX_COLUMN;
  if (LINES > MAX_LINE)
    LINES = MAX_LINE;
}
