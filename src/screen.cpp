#include "screen.h"
#include "terms.h"
#include "alloc.h"
#include "myctype.h"
#include "termentry.h"

static int max_LINES = 0;
static int max_COLS = 0;
static int graph_enabled = 0;
static int tab_step = 8;

static Screen *ScreenElem = {};
static Screen **ScreenImage = {};
static l_prop CurrentMode = {};
static int CurLine;
static int CurColumn;
#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = (ch))
#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))

static void touch_column(int col) {
  if (col >= 0 && col < COLS)
    ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
}

static void touch_line(void) {
  if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
    int i;
    for (i = 0; i < COLS; i++)
      ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
    ScreenImage[CurLine]->isdirty =
        (LineDirtyFlags)(ScreenImage[CurLine]->isdirty | L_DIRTY);
  }
}

static TermEntry _entry;

void setupscreen(const TermEntry &entry) {
  _entry = entry;

  if (LINES + 1 > max_LINES) {
    max_LINES = LINES + 1;
    max_COLS = 0;
    ScreenElem = (Screen *)New_N(Screen, max_LINES);
    ScreenImage = (Screen **)New_N(Screen *, max_LINES);
  }
  if (COLS + 1 > max_COLS) {
    max_COLS = COLS + 1;
    for (int i = 0; i < max_LINES; i++) {
      ScreenElem[i].lineimage = (Utf8 *)New_N(Utf8, max_COLS);
      ScreenElem[i].lineprop = (l_prop *)New_N(l_prop, max_COLS);
    }
  }

  int i;
  for (i = 0; i < LINES; i++) {
    ScreenImage[i] = &ScreenElem[i];
    ScreenImage[i]->lineprop[0] = S_EOL;
    ScreenImage[i]->isdirty = {};
  }
  for (; i < max_LINES; i++) {
    ScreenElem[i].isdirty = L_UNUSED;
  }

  clear();
}

void clear(void) {
  term_writestr(_entry.T_cl);

  int i, j;
  l_prop *p;
  move(0, 0);
  for (i = 0; i < LINES; i++) {
    ScreenImage[i]->isdirty = {};
    p = ScreenImage[i]->lineprop;
    for (j = 0; j < COLS; j++) {
      p[j] = S_EOL;
    }
  }
  CurrentMode = C_ASCII;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void) { /* Clear to the end of line */
  int i;
  auto lprop = ScreenImage[CurLine]->lineprop;

  if (lprop[CurColumn] & S_EOL)
    return;

  if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
      ScreenImage[CurLine]->eol > CurColumn)
    ScreenImage[CurLine]->eol = CurColumn;

  ScreenImage[CurLine]->isdirty =
      (LineDirtyFlags)(ScreenImage[CurLine]->isdirty | L_CLRTOEOL);
  touch_line();
  for (i = CurColumn; i < COLS && !(lprop[i] & S_EOL); i++) {
    lprop[i] = S_EOL | S_DIRTY;
  }
}

void clrtoeolx(void) { clrtoeol(); }

static void clrtobot_eol(void (*clrtoeol)()) {
  int l, c;

  l = CurLine;
  c = CurColumn;
  (*clrtoeol)();
  CurColumn = 0;
  CurLine++;
  for (; CurLine < LINES; CurLine++)
    (*clrtoeol)();
  CurLine = l;
  CurColumn = c;
}

void clrtobot(void) { clrtobot_eol(clrtoeol); }

void clrtobotx(void) { clrtobot_eol(clrtoeolx); }

void addstr(const char *s) {
  while (*s != '\0')
    addch(*(s++));
}

void addnstr(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
}

void addnstr_sup(const char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
  for (; i < n; i++)
    addch(' ');
}

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_GRAPHICS)

/* Charactor Color */
#define COL_FCOLOR 0xf00
#define COL_FBLACK 0x800
#define COL_FRED 0x900
#define COL_FGREEN 0xa00
#define COL_FYELLOW 0xb00
#define COL_FBLUE 0xc00
#define COL_FMAGENTA 0xd00
#define COL_FCYAN 0xe00
#define COL_FWHITE 0xf00
#define COL_FTERM 0x000

const Utf8 SPACE = {' ', 0, 0, 0};
#define M_SPACE (S_SCREENPROP | S_COLORED | S_GRAPHICS)

static int need_redraw(const Utf8 &c1, l_prop pr1, const Utf8 &c2, l_prop pr2) {
  if (c1 != c2)
    return 1;
  if (c1 == Utf8{' '})
    return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

  if ((pr1 ^ pr2) & ~S_DIRTY)
    return 1;

  return 0;
}

void refresh(FILE *ttyf) {
  int line, col, pcol;
  int pline = CurLine;
  int moved = RF_NEED_TO_MOVE;
  Utf8 *pc;
  l_prop *pr, mode = 0;
  LineDirtyFlags *dirty;

  for (line = 0; line <= LASTLINE; line++) {
    dirty = &ScreenImage[line]->isdirty;
    if (*dirty & L_DIRTY) {
      *dirty = (LineDirtyFlags)(*dirty & ~L_DIRTY);
      pc = ScreenImage[line]->lineimage;
      pr = ScreenImage[line]->lineprop;
      for (col = 0; col < COLS && !(pr[col] & S_EOL); col++) {
        if (*dirty & L_NEED_CE && col >= ScreenImage[line]->eol) {
          if (need_redraw(pc[col], pr[col], SPACE, 0))
            break;
        } else {
          if (pr[col] & S_DIRTY)
            break;
        }
      }
      if (*dirty & (L_NEED_CE | L_CLRTOEOL)) {
        pcol = ScreenImage[line]->eol;
        if (pcol >= COLS) {
          *dirty = (LineDirtyFlags)(*dirty & ~(L_NEED_CE | L_CLRTOEOL));
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
          term_write1('\n');
          term_write1('\r');
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
        term_writestr(_entry.T_ce);
        if (col != pcol)
          term_move(line, col);
      }
      pline = line;
      pcol = col;

      term_move(line, col);
      for (; col < COLS;) {
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
        if ((!(pr[col] & S_STANDOUT) && (mode & S_STANDOUT)) ||
            (!(pr[col] & S_UNDERLINE) && (mode & S_UNDERLINE)) ||
            (!(pr[col] & S_BOLD) && (mode & S_BOLD)) ||
            (!(pr[col] & S_COLORED) && (mode & S_COLORED)) ||
            (!(pr[col] & S_GRAPHICS) && (mode & S_GRAPHICS))) {
          if ((mode & S_COLORED))
            term_writestr(_entry.T_op);
          if (mode & S_GRAPHICS)
            term_writestr(_entry.T_ae);
          term_writestr(_entry.T_me);
          mode &= ~M_MEND;
        }

        // if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
        //         ? need_redraw(pc[col], pr[col], SPACE, 0)
        //         : (pr[col] & S_DIRTY))
        {
          if (pcol == col - 1)
            term_writestr(_entry.T_nd);
          else if (pcol != col)
            term_move(line, col);

          if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
            term_writestr(_entry.T_so);
            mode |= S_STANDOUT;
          }
          if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
            term_writestr(_entry.T_us);
            mode |= S_UNDERLINE;
          }
          if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
            term_writestr(_entry.T_md);
            mode |= S_BOLD;
          }
          // if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
          //   color = (pr[col] & COL_FCOLOR);
          //   mode = ((mode & ~COL_FCOLOR) | color);
          //   writestr(color_seq(color));
          // }
          if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
            if (!graph_enabled) {
              graph_enabled = 1;
              term_writestr(_entry.T_eA);
            }
            term_writestr(_entry.T_as);
            mode |= S_GRAPHICS;
          }
          if (pr[col] & S_GRAPHICS) {
            term_write1(term_graphchar(pc[col].b0));
            pcol = col = col + 1;
          } else {
            assert(CHMODE(pr[col]) != C_WCHAR2);
            auto utf8 = pc[col];
            auto view = utf8.view();
            auto width = utf8.width();
            fwrite(view.begin(), view.size(), 1, ttyf);
            pcol = col = col + width;
          }
        }
      } // cols

      if (col == COLS)
        moved = RF_NEED_TO_MOVE;
      for (; col < COLS && !(pr[col] & S_EOL); col++)
        pr[col] |= S_EOL;
    }
    *dirty = (LineDirtyFlags)(*dirty & ~(L_NEED_CE | L_CLRTOEOL));
    if (mode & M_MEND) {
      if (mode & (S_COLORED))
        term_writestr(_entry.T_op);
      if (mode & S_GRAPHICS) {
        term_writestr(_entry.T_ae);
      }
      term_writestr(_entry.T_me);
      mode &= ~M_MEND;
    }
  }
  term_move(CurLine, CurColumn);
  flush_tty();
}
void move(int line, int column) {
  if (line >= 0 && line < LINES)
    CurLine = line;
  if (column >= 0 && column < COLS)
    CurColumn = column;
}

void toggle_stand(void) {
  l_prop *pr = ScreenImage[CurLine]->lineprop;
  pr[CurColumn] ^= S_STANDOUT;
}

// void setfcolor(int color) {
//   CurrentMode &= ~COL_FCOLOR;
//   if ((color & 0xf) <= 7)
//     CurrentMode |= (((color & 7) | 8) << 8);
// }

void bold(void) { CurrentMode |= S_BOLD; }

void boldend(void) { CurrentMode &= ~S_BOLD; }

void underline(void) { CurrentMode |= S_UNDERLINE; }

void underlineend(void) { CurrentMode &= ~S_UNDERLINE; }

void graphstart(void) { CurrentMode |= S_GRAPHICS; }

void graphend(void) { CurrentMode &= ~S_GRAPHICS; }

void standout(void) { CurrentMode |= S_STANDOUT; }

void standend(void) { CurrentMode &= ~S_STANDOUT; }

void wrap(void) {
  if (CurLine == LASTLINE)
    return;
  CurLine++;
  CurColumn = 0;
}

void addch(char c) { addmch({(char8_t)c, 0, 0, 0}); }

void addmch(const Utf8 &utf8) {
  if (CurColumn == COLS)
    wrap();
  if (CurColumn >= COLS)
    return;
  auto p = ScreenImage[CurLine]->lineimage;
  auto pr = ScreenImage[CurLine]->lineprop;

  if (pr[CurColumn] & S_EOL) {
    if (utf8 == SPACE && !(CurrentMode & M_SPACE)) {
      CurColumn++;
      return;
    }
    for (int i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
    }
  }

  auto c = utf8.b0;
  if (c == '\t' || c == '\n' || c == '\r' || c == '\b')
    SETCHMODE(CurrentMode, C_CTRL);
  else if (utf8.view().size() > 1)
    SETCHMODE(CurrentMode, C_WCHAR1);
  else if (!IS_CNTRL(c))
    SETCHMODE(CurrentMode, C_ASCII);
  else
    return;

  int width = utf8.cols();
  /* Required to erase bold or underlined character for some * terminal
   * emulators. */
  int i = CurColumn + width - 1;
  if (i < COLS &&
      (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], utf8, CurrentMode)) ||
       ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
    touch_line();
    i++;
    if (i < COLS) {
      touch_column(i);
      if (pr[i] & S_EOL) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
      } else {
        for (i++; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++)
          touch_column(i);
      }
    }
  }

  if (CurColumn + width > COLS) {
    touch_line();
    for (i = CurColumn; i < COLS; i++) {
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      touch_column(i);
    }
    wrap();
    if (CurColumn + width > COLS)
      return;
    p = ScreenImage[CurLine]->lineimage;
    pr = ScreenImage[CurLine]->lineprop;
  }
  if (CHMODE(pr[CurColumn]) == C_WCHAR2) {
    touch_line();
    for (i = CurColumn - 1; i >= 0; i--) {
      l_prop l = CHMODE(pr[i]);
      SETCH(p[i], SPACE, 1);
      SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
      touch_column(i);
      if (l != C_WCHAR2)
        break;
    }
  }
  if (CHMODE(CurrentMode) != C_CTRL) {
    if (need_redraw(p[CurColumn], pr[CurColumn], utf8, CurrentMode)) {
      SETCH(p[CurColumn], utf8, len);
      SETPROP(pr[CurColumn], CurrentMode);
      touch_line();
      touch_column(CurColumn);
      SETCHMODE(CurrentMode, C_WCHAR2);
      for (i = CurColumn + 1; i < CurColumn + width; i++) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[CurColumn] & ~C_WHICHCHAR) | C_WCHAR2);
        touch_column(i);
      }
      for (; i < COLS && CHMODE(pr[i]) == C_WCHAR2; i++) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & ~C_WHICHCHAR) | C_ASCII);
        touch_column(i);
      }
    }
    CurColumn += width;
  } else if (c == '\t') {
    auto dest = (CurColumn + tab_step) / tab_step * tab_step;
    if (dest >= COLS) {
      wrap();
      touch_line();
      dest = tab_step;
      p = ScreenImage[CurLine]->lineimage;
      pr = ScreenImage[CurLine]->lineprop;
    }
    for (i = CurColumn; i < dest; i++) {
      if (need_redraw(p[i], pr[i], SPACE, CurrentMode)) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], CurrentMode);
        touch_line();
        touch_column(i);
      }
    }
    CurColumn = i;
  } else if (c == '\n') {
    wrap();
  } else if (c == '\r') { /* Carriage return */
    CurColumn = 0;
  } else if (c == '\b' && CurColumn > 0) { /* Backspace */
    CurColumn--;
    while (CurColumn > 0 && CHMODE(pr[CurColumn]) == C_WCHAR2)
      CurColumn--;
  }
}
