#include "scr.h"
#include "alloc.h"
#include "terms.h"
#include "myctype.h"

static char *color_seq(int colmode) {
  static char seqbuf[32];
  sprintf(seqbuf, "\033[%dm", ((colmode >> 8) & 7) + 30);
  return seqbuf;
}

static int tab_step = 8;

static int graph_enabled = 0;

typedef unsigned short l_prop;

/* Screen properties */
#define S_SCREENPROP 0x0f
#define S_NORMAL 0x00
#define S_STANDOUT 0x01
#define S_UNDERLINE 0x02
#define S_BOLD 0x04
#define S_EOL 0x08

/* Sort of Character */
#define C_WHICHCHAR 0xc0
#define C_ASCII 0x00
#define C_CTRL 0xc0

/* Line status */
#define L_DIRTY 0x01
#define L_UNUSED 0x02
#define L_NEED_CE 0x04
#define L_CLRTOEOL 0x08

#define RF_NEED_TO_MOVE 0
#define RF_CR_OK 1
#define RF_NONEED_TO_MOVE 2
#define M_MEND (S_STANDOUT | S_UNDERLINE | S_BOLD | S_COLORED | S_GRAPHICS)

#define CHMODE(c) ((c) & C_WHICHCHAR)
#define SETCHMODE(var, mode) ((var) = (((var) & ~C_WHICHCHAR) | mode))
#define SETCH(var, ch, len) ((var) = (ch))

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

#define ISDIRTY(d) ((d) & L_DIRTY)
#define ISUNUSED(d) ((d) & L_UNUSED)
#define NEED_CE(d) ((d) & L_NEED_CE)

#define S_COLORED 0xf00
#define S_GRAPHICS 0x10
#define S_DIRTY 0x20
#define SETPROP(var, prop) (var = (((var) & S_DIRTY) | prop))
#define M_SPACE (S_SCREENPROP | S_COLORED | S_GRAPHICS)
#define M_CEOL (~(M_SPACE | C_WHICHCHAR))
#define SPACE ' '

static int max_LINES = 0;
static int max_COLS = 0;

typedef struct scline {
  char *lineimage;
  l_prop *lineprop;
  short isdirty;
  short eol;
} Screen;

static int CurLine, CurColumn;
static Screen *ScreenElem = NULL, **ScreenImage = NULL;
static l_prop CurrentMode = 0;

static int need_redraw(char c1, l_prop pr1, char c2, l_prop pr2) {
  if (c1 != c2)
    return 1;
  if (c1 == ' ')
    return (pr1 ^ pr2) & M_SPACE & ~S_DIRTY;

  if ((pr1 ^ pr2) & ~S_DIRTY)
    return 1;

  return 0;
}

void setupscreen(int LINES, int COLS) {
  int i;

  if (LINES + 1 > max_LINES) {
    max_LINES = LINES + 1;
    max_COLS = 0;
    ScreenElem = New_N(Screen, max_LINES);
    ScreenImage = New_N(Screen *, max_LINES);
  }
  if (COLS + 1 > max_COLS) {
    max_COLS = COLS + 1;
    for (i = 0; i < max_LINES; i++) {
      ScreenElem[i].lineimage = New_N(char, max_COLS);
      ScreenElem[i].lineprop = New_N(l_prop, max_COLS);
    }
  }
  for (i = 0; i < LINES; i++) {
    ScreenImage[i] = &ScreenElem[i];
    ScreenImage[i]->lineprop[0] = S_EOL;
    ScreenImage[i]->isdirty = 0;
  }
  for (; i < max_LINES; i++) {
    ScreenElem[i].isdirty = L_UNUSED;
  }

  clear();
}

void clear(void) {
  term_clear();
  move(0, 0);
  for (int i = 0; i < LINES; i++) {
    ScreenImage[i]->isdirty = 0;
    auto p = ScreenImage[i]->lineprop;
    for (int j = 0; j < COLS; j++) {
      p[j] = S_EOL;
    }
  }
  CurrentMode = C_ASCII;
}

#define graphchar(c)                                                           \
  (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? gcmap[(c) - ' '] : (c))

void refresh() {
  int line, col, pcol;
  int pline = CurLine;
  int moved = RF_NEED_TO_MOVE;
  char *pc;
  l_prop *pr, mode = 0;
  l_prop color = COL_FTERM;
  short *dirty;

  for (line = 0; line <= LASTLINE; line++) {
    dirty = &ScreenImage[line]->isdirty;
    if (*dirty & L_DIRTY) {
      *dirty &= ~L_DIRTY;
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
          term_putc('\n');
          term_putc('\r');
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
        term_puts(T_ce);
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
            term_puts(T_op);
          if (mode & S_GRAPHICS)
            term_puts(T_ae);
          term_puts(T_me);
          mode &= ~M_MEND;
        }
        if ((*dirty & L_NEED_CE && col >= ScreenImage[line]->eol)
                ? need_redraw(pc[col], pr[col], SPACE, 0)
                : (pr[col] & S_DIRTY)) {
          if (pcol == col - 1)
            term_puts(T_nd);
          else if (pcol != col)
            term_move(line, col);

          if ((pr[col] & S_STANDOUT) && !(mode & S_STANDOUT)) {
            term_puts(T_so);
            mode |= S_STANDOUT;
          }
          if ((pr[col] & S_UNDERLINE) && !(mode & S_UNDERLINE)) {
            term_puts(T_us);
            mode |= S_UNDERLINE;
          }
          if ((pr[col] & S_BOLD) && !(mode & S_BOLD)) {
            term_puts(T_md);
            mode |= S_BOLD;
          }
          if ((pr[col] & S_COLORED) && (pr[col] ^ mode) & COL_FCOLOR) {
            color = (pr[col] & COL_FCOLOR);
            mode = ((mode & ~COL_FCOLOR) | color);
            term_puts(color_seq(color));
          }
          if ((pr[col] & S_GRAPHICS) && !(mode & S_GRAPHICS)) {
            if (!graph_enabled) {
              graph_enabled = 1;
              term_puts(T_eA);
            }
            term_puts(T_as);
            mode |= S_GRAPHICS;
          }
          term_putc((pr[col] & S_GRAPHICS) ? graphchar(pc[col]) : pc[col]);
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
        term_puts(T_op);
      if (mode & S_GRAPHICS) {
        term_puts(T_ae);
      }
      term_puts(T_me);
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

void touch_line(void) {
  if (!(ScreenImage[CurLine]->isdirty & L_DIRTY)) {
    int i;
    for (i = 0; i < COLS; i++)
      ScreenImage[CurLine]->lineprop[i] &= ~S_DIRTY;
    ScreenImage[CurLine]->isdirty |= L_DIRTY;
  }
}

void touch_column(int col) {
  if (col >= 0 && col < COLS)
    ScreenImage[CurLine]->lineprop[col] |= S_DIRTY;
}

void wrap(void) {
  if (CurLine == LASTLINE)
    return;
  CurLine++;
  CurColumn = 0;
}

static void clrtobot_eol(void (*clrtoeol)()) {
  int l = CurLine;
  int c = CurColumn;
  (*clrtoeol)();
  CurColumn = 0;
  CurLine++;
  for (; CurLine < LINES; CurLine++)
    (*clrtoeol)();
  CurLine = l;
  CurColumn = c;
}

/* XXX: conflicts with curses's clrtoeol(3) ? */
void clrtoeol(void) { /* Clear to the end of line */
  int i;
  l_prop *lprop = ScreenImage[CurLine]->lineprop;

  if (lprop[CurColumn] & S_EOL)
    return;

  if (!(ScreenImage[CurLine]->isdirty & (L_NEED_CE | L_CLRTOEOL)) ||
      ScreenImage[CurLine]->eol > CurColumn)
    ScreenImage[CurLine]->eol = CurColumn;

  ScreenImage[CurLine]->isdirty |= L_CLRTOEOL;
  touch_line();
  for (i = CurColumn; i < COLS && !(lprop[i] & S_EOL); i++) {
    lprop[i] = S_EOL | S_DIRTY;
  }
}

void clrtoeolx(void) { clrtoeol(); }
void clrtobot(void) { clrtobot_eol(clrtoeol); }
void clrtobotx(void) { clrtobot_eol(clrtoeolx); }

void addch(char pc) {
  l_prop *pr;
  int dest, i;
  char *p;
  char c = pc;

  if (CurColumn == COLS)
    wrap();
  if (CurColumn >= COLS)
    return;
  p = ScreenImage[CurLine]->lineimage;
  pr = ScreenImage[CurLine]->lineprop;

  /* Eliminate unprintables according to * iso-8859-*.
   * Particularly 0x96 messes up T.Dickey's * (xfree-)xterm */
  if (IS_INTSPACE(c))
    c = ' ';

  if (pr[CurColumn] & S_EOL) {
    if (c == ' ' && !(CurrentMode & M_SPACE)) {
      CurColumn++;
      return;
    }
    for (i = CurColumn; i >= 0 && (pr[i] & S_EOL); i--) {
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
  i = CurColumn;
  if (i < COLS &&
      (((pr[i] & S_BOLD) && need_redraw(p[i], pr[i], pc, CurrentMode)) ||
       ((pr[i] & S_UNDERLINE) && !(CurrentMode & S_UNDERLINE)))) {
    touch_line();
    i++;
    if (i < COLS) {
      touch_column(i);
      if (pr[i] & S_EOL) {
        SETCH(p[i], SPACE, 1);
        SETPROP(pr[i], (pr[i] & M_CEOL) | C_ASCII);
      }
    }
  }

  if (CHMODE(CurrentMode) != C_CTRL) {
    if (need_redraw(p[CurColumn], pr[CurColumn], pc, CurrentMode)) {
      SETCH(p[CurColumn], pc, len);
      SETPROP(pr[CurColumn], CurrentMode);
      touch_line();
      touch_column(CurColumn);
    }
    CurColumn++;
  } else if (c == '\t') {
    dest = (CurColumn + tab_step) / tab_step * tab_step;
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
  }
}

void addstr(char *s) {
  while (*s != '\0')
    addch(*(s++));
}

void addnstr(char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
}

void addnstr_sup(char *s, int n) {
  int i;
  for (i = 0; i < n && *s != '\0'; i++)
    addch(*(s++));
  for (; i < n; i++)
    addch(' ');
}

void standout(void) { CurrentMode |= S_STANDOUT; }

void standend(void) { CurrentMode &= ~S_STANDOUT; }

void toggle_stand(void) {
  l_prop *pr = ScreenImage[CurLine]->lineprop;
  pr[CurColumn] ^= S_STANDOUT;
}

void bold(void) { CurrentMode |= S_BOLD; }

void boldend(void) { CurrentMode &= ~S_BOLD; }

void underline(void) { CurrentMode |= S_UNDERLINE; }

void underlineend(void) { CurrentMode &= ~S_UNDERLINE; }

void graphstart(void) { CurrentMode |= S_GRAPHICS; }

void graphend(void) { CurrentMode &= ~S_GRAPHICS; }
