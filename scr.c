#include "scr.h"
#include "Str.h"
#include "alloc.h"
#include "termsize.h"
#include "myctype.h"

static int tab_step = 8;

#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

static int max_LINES = 0;
static int max_COLS = 0;
static struct ScreenLine *ScreenElem = NULL;
static l_prop CurrentMode = 0;

static struct Screen g_scr;
struct Screen *scr_get() { return &g_scr; }

void scr_setup(int LINES, int COLS) {

  if (LINES + 1 > max_LINES) {
    max_LINES = LINES + 1;
    max_COLS = 0;
    ScreenElem = New_N(struct ScreenLine, max_LINES);
    g_scr.ScreenImage = New_N(struct ScreenLine *, max_LINES);
  }
  if (COLS + 1 > max_COLS) {
    max_COLS = COLS + 1;
    for (int i = 0; i < max_LINES; i++) {
      ScreenElem[i].lineimage = New_N(struct Utf8, max_COLS);
      ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
    }
  }

  {
    int i;
    for (i = 0; i < LINES; i++) {
      g_scr.ScreenImage[i] = &ScreenElem[i];
      g_scr.ScreenImage[i]->lineprop[0] = SCREEN_EOL;
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
      p[j] = SCREEN_EOL;
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
      g_scr.ScreenImage[g_scr.CurLine]->lineprop[i] &= ~SCREEN_DIRTY;
    g_scr.ScreenImage[g_scr.CurLine]->isdirty |= L_DIRTY;
  }
}

void scr_touch_column(int col) {
  if (col >= 0 && col < COLS)
    g_scr.ScreenImage[g_scr.CurLine]->lineprop[col] |= SCREEN_DIRTY;
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

  if (lprop[g_scr.CurColumn] & SCREEN_EOL)
    return;

  if (!(g_scr.ScreenImage[g_scr.CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
      g_scr.ScreenImage[g_scr.CurLine]->eol > g_scr.CurColumn)
    g_scr.ScreenImage[g_scr.CurLine]->eol = g_scr.CurColumn;

  g_scr.ScreenImage[g_scr.CurLine]->isdirty |= L_CLRTOEOL;
  scr_touch_line();
  for (i = g_scr.CurColumn; i < COLS && !(lprop[i] & SCREEN_EOL); i++) {
    lprop[i] = SCREEN_EOL | SCREEN_DIRTY;
  }
}

void scr_clrtoeolx(void) { scr_clrtoeol(); }
void scr_clrtobot(void) { scr_clrtobot_eol(scr_clrtoeol); }
void scr_clrtobotx(void) { scr_clrtobot_eol(scr_clrtoeolx); }

bool scr_need_redraw(struct Utf8 c1, l_prop pr1, struct Utf8 c2, l_prop pr2) {
  if (!utf8sequence_equals(c1, c2)) {
    return 1;
  }
  if (c1.c0 == ' ') {
    return (pr1 ^ pr2) & M_SPACE & ~SCREEN_DIRTY;
  }

  if ((pr1 ^ pr2) & ~SCREEN_DIRTY) {
    return 1;
  }

  return 0;
}

void scr_addutf8(const uint8_t *utf8) {
  char c = *utf8;
  int width = utf8sequence_width(utf8);
  struct Utf8 pc;
  int len = utf8sequence_from_str(utf8, &pc);

  static Str tmp = NULL;
  if (tmp == NULL)
    tmp = Strnew();
  Strcopy_charp_n(tmp, (char *)utf8, len);

  if (g_scr.CurColumn == COLS)
    scr_wrap();
  if (g_scr.CurColumn >= COLS)
    return;
  auto p = g_scr.ScreenImage[g_scr.CurLine]->lineimage;
  auto pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;

  if (pr[g_scr.CurColumn] & SCREEN_EOL) {
    if (c == ' ' && !(CurrentMode & M_SPACE)) {
      g_scr.CurColumn++;
      return;
    }
    for (int i = g_scr.CurColumn; i >= 0 && (pr[i] & SCREEN_EOL); i--) {
      p[i] = SPACE;
      SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
    }
  }

  if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
    SETCHMODE(CurrentMode, C_CTRL);
  else if (len > 1)
    SETCHMODE(CurrentMode, C_WCHAR1);
  else if (!IS_CNTRL(c))
    SETCHMODE(CurrentMode, C_ASCII);
  else
    return;

  /* Required to erase bold or underlined character for some * terminal
   * emulators. */
  int i = g_scr.CurColumn + width - 1;
  if (i < COLS &&
      (((pr[i] & SCREEN_BOLD) &&
        scr_need_redraw(p[i], pr[i], pc, CurrentMode)) ||
       ((pr[i] & SCREEN_UNDERLINE) && !(CurrentMode & SCREEN_UNDERLINE)))) {
    scr_touch_line();
    i++;
    if (i < COLS) {
      scr_touch_column(i);
      if (pr[i] & SCREEN_EOL) {
        p[i] = SPACE;
        SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
      } else {
        for (i++; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
          scr_touch_column(i);
      }
    }
  }

  if (g_scr.CurColumn + width > COLS) {
    scr_touch_line();
    for (i = g_scr.CurColumn; i < COLS; i++) {
      p[i] = SPACE;
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      scr_touch_column(i);
    }
    scr_wrap();
    if (g_scr.CurColumn + width > COLS)
      return;
    p = g_scr.ScreenImage[g_scr.CurLine]->lineimage;
    pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;
  }
  if (CHMODE(pr[g_scr.CurColumn]) == C_WCHAR2) {
    scr_touch_line();
    for (i = g_scr.CurColumn - 1; i >= 0; i--) {
      l_prop l = CHMODE(pr[i]);
      p[i] = SPACE;
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      scr_touch_column(i);
      if (l != C_WCHAR2)
        break;
    }
  }
  if (CHMODE(CurrentMode) != C_CTRL) {
    if (scr_need_redraw(p[g_scr.CurColumn], pr[g_scr.CurColumn], pc,
                        CurrentMode)) {
      //
      p[g_scr.CurColumn] = pc;
      //
      SETPROP(pr[g_scr.CurColumn], CurrentMode);
      scr_touch_line();
      scr_touch_column(g_scr.CurColumn);
      SETCHMODE(CurrentMode, C_WCHAR2);
      for (i = g_scr.CurColumn + 1; i < g_scr.CurColumn + width; i++) {
        p[i] = SPACE;
        SETPROP(pr[i], (pr[g_scr.CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
        scr_touch_column(i);
      }
      for (; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++) {
        p[i] = SPACE;
        SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
        scr_touch_column(i);
      }
    }
    g_scr.CurColumn += width;
  } else if (c == '\t') {
    int dest = (g_scr.CurColumn + tab_step) / tab_step * tab_step;
    if (dest >= COLS) {
      scr_wrap();
      scr_touch_line();
      dest = tab_step;
      p = g_scr.ScreenImage[g_scr.CurLine]->lineimage;
      pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;
    }
    for (i = g_scr.CurColumn; i < dest; i++) {
      if (scr_need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
        p[i] = SPACE;
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
    while (g_scr.CurColumn > 0 && CHMODE(pr[g_scr.CurColumn]) == C_WCHAR2)
      g_scr.CurColumn--;
  }
}

void scr_addch(char pc) { scr_addutf8((const uint8_t *)&pc); }

void scr_addstr(const char *s) {
  auto p = s;
  while (*p) {
    auto len = utf8sequence_len((const uint8_t *)p);
    if (len == 0) {
      break;
    }
    scr_addutf8((const uint8_t *)p);
    p += len;
  }
}

void scr_addnstr(const char *s, int n) {
  for (int i = 0; i < n;) {
    if (!s[i]) {
      break;
    }
    auto len = utf8sequence_len((const uint8_t *)s + i);
    if (len == 0) {
      break;
    }
    scr_addutf8((const uint8_t *)s + i);
    i += len;
  }
}

void scr_addnstr_sup(const char *s, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (!s[i]) {
      break;
    }
    auto len = utf8sequence_len((const uint8_t *)s + i);
    if (len == 0) {
      break;
    }
    scr_addutf8((const uint8_t *)s + i);
    i += len;
  }
  for (; i < n; i++) {
    scr_addch(' ');
  }
}

void scr_standout(void) { 
  //
  // CurrentMode |= SCREEN_STANDOUT; 
}

void scr_standend(void) { 
  //
  CurrentMode &= ~SCREEN_STANDOUT; 
}

void scr_toggle_stand(void) {
  l_prop *pr = g_scr.ScreenImage[g_scr.CurLine]->lineprop;
  pr[g_scr.CurColumn] ^= SCREEN_STANDOUT;
}

void scr_bold(void) {
  //
  CurrentMode |= SCREEN_BOLD;
}

void scr_boldend(void) {
  //
  CurrentMode &= ~SCREEN_BOLD;
}

void scr_underline(void) {
  //
  CurrentMode |= SCREEN_UNDERLINE;
}

void scr_underlineend(void) {
  //
  CurrentMode &= ~SCREEN_UNDERLINE;
}

void scr_graphstart(void) {
  //
  CurrentMode |= SCREEN_GRAPHICS;
}

void scr_graphend(void) {
  //
  CurrentMode &= ~SCREEN_GRAPHICS;
}

void scr_message(const char *s, int return_x, int return_y) {
  scr_move(LASTLINE, 0);
  scr_addnstr(s, COLS - 1);
  scr_clrtoeolx();
  scr_move(return_y, return_x);
}
