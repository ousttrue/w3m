#include "termcap_load.h"
#include "termcap_getentry.h"
#include "termcap_entry.h"

static TermCap s_termcap;

#define CSI "0x1b["

bool termcap_load(TermCap *t, const char *ent) {
  if (tgetent(t->bp, ent) != 1) {
    return false;
  }

  auto pt = t->funcstr;
  t->T_ce = tgetstr("ce", &pt);
  t->T_cd = tgetstr("cd", &pt);
  t->T_kr = tgetstr("nd", &pt);
  if (!t->T_kr) {
    t->T_kr = tgetstr("kr", &pt);
  }
  if (tgetflag("bs")) {
    t->T_kl = "\b";
  } else {
    t->T_kl = tgetstr("le", &pt);
    if (!t->T_kl) {
      t->T_kl = tgetstr("kb", &pt);
    }
    if (!t->T_kl) {
      t->T_kl = tgetstr("kl", &pt);
    }
  }
  t->T_cr = tgetstr("cr", &pt);
  t->T_ta = tgetstr("ta", &pt);
  t->T_sc = tgetstr("sc", &pt);
  t->T_rc = tgetstr("rc", &pt);
  t->T_so = tgetstr("so", &pt);
  t->T_se = tgetstr("se", &pt);
  t->T_us = tgetstr("us", &pt);
  t->T_ue = tgetstr("ue", &pt);
  t->T_md = tgetstr("md", &pt);
  t->T_me = tgetstr("me", &pt);
  t->T_cl = tgetstr("cl", &pt);
  t->T_cm = tgetstr("cm", &pt);
  t->T_al = tgetstr("al", &pt);
  t->T_sr = tgetstr("sr", &pt);
  t->T_ti = tgetstr("ti", &pt);
  t->T_te = tgetstr("te", &pt);
  t->T_nd = tgetstr("nd", &pt);
  t->T_eA = tgetstr("eA", &pt);
  t->T_as = tgetstr("as", &pt);
  t->T_ae = tgetstr("ae", &pt);
  t->T_ac = tgetstr("ac", &pt);
  t->T_op = tgetstr("op", &pt);
  t->LINES = tgetnum("li");
  t->COLS = tgetnum("co");
  return true;
}

void termcap_load_xterm(TermCap *entry) {
  entry->T_cd = CSI "J";
  entry->T_ce = CSI "K";
}
