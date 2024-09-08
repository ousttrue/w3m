#include "termcap_entry.h"
#include "indep.h"
#include "termcap.h"

#define MAX_LINE 200
#define MAX_COLUMN 400

int LINES;
int COLS;

static TermCap s_termcap;
TermCap *termcap_get() { return &s_termcap; };

#define GETSTR(v, s)                                                           \
  {                                                                            \
    v = pt;                                                                    \
    suc = tgetstr(s, &pt);                                                     \
    if (!suc)                                                                  \
      v = "";                                                                  \
    else                                                                       \
      v = allocStr(suc, -1);                                                   \
  }

bool termcap_load(const char *ent) {
  if (tgetent(s_termcap.bp, ent) != 1) {
    return false;
  }

  auto t = &s_termcap;
  char funcstr[256];
  char *pt = funcstr;
  char *suc;
  GETSTR(t->T_ce, "ce"); /* clear to the end of line */
  GETSTR(t->T_cd, "cd"); /* clear to the end of display */
  GETSTR(t->T_kr, "nd"); /* cursor right */
  if (suc == NULL)
    GETSTR(t->T_kr, "kr");
  if (tgetflag("bs"))
    t->T_kl = "\b"; /* cursor left */
  else {
    GETSTR(t->T_kl, "le");
    if (suc == NULL)
      GETSTR(t->T_kl, "kb");
    if (suc == NULL)
      GETSTR(t->T_kl, "kl");
  }
  GETSTR(t->T_cr, "cr");    /* carriage return */
  GETSTR(t->T_ta, "ta");    /* tab */
  GETSTR(t->T_sc, "sc");    /* save cursor */
  GETSTR(t->T_rc, "rc");    /* restore cursor */
  GETSTR(t->T_so, "so");    /* standout mode */
  GETSTR(t->T_se, "se");    /* standout mode end */
  GETSTR(t->T_us, "us");    /* underline mode */
  GETSTR(t->T_ue, "ue");    /* underline mode end */
  GETSTR(t->T_md, "md");    /* bold mode */
  GETSTR(t->T_me, "me");    /* bold mode end */
  GETSTR(t->T_cl, "cl");    /* clear screen */
  GETSTR(t->T_cm, "cm");    /* cursor move */
  GETSTR(t->T_al, "al");    /* append line */
  GETSTR(t->T_sr, "sr");    /* scroll reverse */
  GETSTR(t->T_ti, "ti");    /* terminal init */
  GETSTR(t->T_te, "te");    /* terminal end */
  GETSTR(t->T_nd, "nd");    /* move right one space */
  GETSTR(t->T_eA, "eA");    /* enable alternative charset */
  GETSTR(t->T_as, "as");    /* alternative (graphic) charset start */
  GETSTR(t->T_ae, "ae");    /* alternative (graphic) charset end */
  GETSTR(t->T_ac, "ac");    /* graphics charset pairs */
  GETSTR(t->T_op, "op");    /* set default color pair to its original value */
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
