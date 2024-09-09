#include "scr.h"
#include "alloc.h"
#include "termseq/termcap_entry.h"
#include "myctype.h"

static int tab_step = 8;

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = (ch))

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

static int max_LINES = 0;
static int max_COLS = 0;
static Screen *ScreenElem = NULL;
static l_prop CurrentMode = 0;

static Scr g_scr;
Scr *scr_get() { return &g_scr; }

void scr_setup(int LINES, int COLS) {

  if (LINES + 1 > max_LINES) {
    max_LINES = LINES + 1;
    max_COLS = 0;
    ScreenElem = New_N(Screen, max_LINES);
    g_scr.ScreenImage = New_N(Screen *, max_LINES);
  }
  if (COLS + 1 > max_COLS) {
    max_COLS = COLS + 1;
    for (int i = 0; i < max_LINES; i++) {
      ScreenElem[i].lineimage = New_N(char, max_COLS);
      ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
    }
  }

  {
    int i;
    for (i = 0; i < LINES; i++) {
      g_scr.ScreenImage[i] = &ScreenElem[i];
      g_scr.ScreenImage[i]->lineprop[0] = S_EOL;
      g_scr.ScreenImage[i]->isdirty = 0;
    }
    for (; i < max_LINES; i++) {
      ScreenElem[i].isdirty = L_UNUSED;
    }
  }

  scr_clear();
}

void scr_clear(void) {
  scr_move(0, 0);
  for (int i = 0; i < LINES; i++) {
    g_scr.ScreenImage[i]->isdirty = 0;
    auto p = g_scr.ScreenImage[i]->lineprop;
    for (int j = 0; j < COLS; j++) {
      p[j] = S_EOL;
    }
  }
  CurrentMode = C_ASCII;
}

void scr_move(int line, int column) {
  if (line >= 0 && line < LINES)
    g_scr.CurLine = line;
  if (column >= 0 && column < COLS)
    g_scr.CurColumn = column;
}

void scr_touch_line(void) {
  if (!(g_scr.ScreenImage[g_scr.CurLine]->isdirty & L_DIRTY)) {
    int i;
    for (i = 0; i < COLS; i++)
      g_scr.ScreenImage[g_scr.CurLine]->lineprop[i] &= ~S_DIRTY;
    g_scr.ScreenImage[g_scr.CurLine]->isdirty |= L_DIRTY;
  }
}

void scr_touch_column(int col) {
  if (col >= 0 && col < COLS)
    g_scr.ScreenImage[g_scr.CurLine]->lineprop[col] |= S_DIRTY;
}

void scr_wrap(void) {
  if (g_scr.CurLine == LASTLINE)
    return;
  g_scr.CurLine++;
  g_scr.CurColumn = 0;
}

static void scr_clrtobot_eol(void (*clrtoeol)()) {
  int l = g_scr.CurLine;
  int c = g_scr.CurColumn;
  (*clrtoeol)();
  g_scr.CurColumn = 0;
  g_scr.CurLine++;
  for (; g_scr.CurLine < LINES; g_scr.CurLine++)
    (*clrtoeol)();
  g_scr.CurLine = l;
  g_scr.CurColumn = c;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void scr_clrtoeol(void) { /* Clear to the end of line */
  int i;
  l_prop *lprop = g_scr.ScreenImage[g_scr.CurLine]->lineprop;

  if (lprop[g_scr.CurColumn] & S_EOL)
    return;

  if (!(g_scr.ScreenImage[g_scr.CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
      g_scr.ScreenImage[g_scr.CurLine]->eol > g_scr.CurColumn)
    g_scr.ScreenImage[g_scr.CurLine]->eol = g_scr.CurColumn;

  g_scr.ScreenImage[g_scr.CurLine]->isdirty |= L_CLRTOEOL;
  scr_touch_line();
  for (i = g_scr.CurColumn; i < COLS && !(lprop[i] & S_EOL); i++) {
    lprop[i] = S_EOL | S_DIRTY;
  }
}

void scr_clrtoeolx(void) { scr_clrtoeol(); }
void scr_clrtobot(void) { scr_clrtobot_eol(scr_clrtoeol); }
void scr_clrtobotx(void) { scr_clrtobot_eol(scr_clrtoeolx); }

int scr_need_redraw(char c1, l_prop pr1, char c2, l_prop pr2) {
  if (c1 != c2)
    return 1;
  if (c1 == ' ')
    return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

  if ((pr1 ^ pr2) & ~S_DIRTY)
    return 1;

  return 0;
}

void scr_addch(char pc) {
  l_prop *pr;
  int dest, i;
  char *p;
  char c = pc;

  if (g_scr.CurColumn == COLS)
    scr_wrap();
  if (g_scr.CurColumn >= COLS)
    return;
  p = g_scr.ScreenImage[g_scr.CurLine]->lineimage;
  pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;

  /* Eliminate unprintables according to * iso-8859-*.
   * Particularly 0x96 messes up T.Dickey's * (xfree-)xterm */
  if (IS_INTSPACE(c))
    c = ' ';

  if (pr[g_scr.CurColumn] & S_EOL) {
    if (c == ' ' && !(CurrentMode & M_SPACE)) {
      g_scr.CurColumn++;
      return;
    }
    for (i = g_scr.CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
    }
  }

  if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
    SETCHMODE(CurrentMode, C_CTRL);
  else if (!IS_CNTRL(c))
    SETCHMODE(CurrentMode, C_ASCII);
  else
    return;

  /* Required to erase bold or underlined character for some * terminal
   * emulators. */
  i = g_scr.CurColumn;
  if (i < COLS &&
      (((pr[i] & S_BOLD) && scr_need_redraw(p[i], pr[i], pc, CurrentMode)) ||
       ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
    scr_touch_line();
    i++;
    if (i < COLS) {
      scr_touch_column(i);
      if (pr[i] & S_EOL) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
      }
    }
  }

  if (CHMODE(CurrentMode) != C_CTRL) {
    if (scr_need_redraw(p[g_scr.CurColumn], pr[g_scr.CurColumn], pc,
                        CurrentMode)) {
      SETCH(p[g_scr.CurColumn], pc, len);
      SETPROP(pr[g_scr.CurColumn], CurrentMode);
      scr_touch_line();
      scr_touch_column(g_scr.CurColumn);
    }
    g_scr.CurColumn++;
  } else if (c == '\t') {
    dest = (g_scr.CurColumn + tab_step) / tab_step * tab_step;
    if (dest >= COLS) {
      scr_wrap();
      scr_touch_line();
      dest = tab_step;
      p = g_scr.ScreenImage[g_scr.CurLine]->lineimage;
      pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;
    }
    for (i = g_scr.CurColumn; i < dest; i++) {
      if (scr_need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], CurrentMode);
        scr_touch_line();
        scr_touch_column(i);
      }
    }
    g_scr.CurColumn = i;
  } else if (c == '\n') {
    scr_wrap();
  } else if (c == '\r') { /* Carriage return */
    g_scr.CurColumn = 0;
  } else if (c == '\b' && g_scr.CurColumn > 0) { /* Backspace */
    g_scr.CurColumn--;
  }
}

void scr_addstr(const char *s) {
  while (*s != '\0')
    scr_addch(*(s++));
}

void scr_addnstr(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    scr_addch(*(s++));
}

void scr_addnstr_sup(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    scr_addch(*(s++));
  for (; i < n; i++)
    scr_addch(' ');
}

void scr_standout(void) { CurrentMode |= S_STANDOUT; }

void scr_standend(void) { CurrentMode &= ~S_STANDOUT; }

void scr_toggle_stand(void) {
  l_prop *pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;
  pr[g_scr.CurColumn] ^= S_STANDOUT;
}

void scr_bold(void) { CurrentMode |= S_BOLD; }

void scr_boldend(void) { CurrentMode &= ~S_BOLD; }

void scr_underline(void) { CurrentMode |= S_UNDERLINE; }

void scr_underlineend(void) { CurrentMode &= ~S_UNDERLINE; }

void scr_graphstart(void) { CurrentMode |= S_GRAPHICS; }

void scr_graphend(void) { CurrentMode &= ~S_GRAPHICS; }

// void setfcolor(int color) {
//   CurrentMode &= ~COL_FCOLOR;
//   if ((color & 0xf) <= 7)
//     CurrentMode |= (((color & 7) | 8) << 8);
// }

void scr_message(const char *s, int return_x, int return_y) {
  // if (!fmInitialized)
  //   return;
  scr_move(LASTLINE, 0);
  scr_addnstr(s, COLS - 1);
  scr_clrtoeolx();
  scr_move(return_y, return_x);
}
