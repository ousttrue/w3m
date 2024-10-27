#include "buffer/viewport.h"
#include "buffer/line.h"
#include "term/scr.h"
#include "text/ctrlcode.h"
#include "text/symbol.h"

bool nextpage_topline = false;

/*
 * effects
 */
static bool ulmode = false;
static bool somode = false;
static bool bomode = false;
static bool anch_mode = false;
static bool emph_mode = false;
static bool imag_mode = false;
static bool form_mode = false;
static bool active_mode = false;
static bool visited_mode = false;
static bool mark_mode = false;
static bool graph_mode = false;

static void clear_effects() {
  if (somode) {
    somode = false;
    scr_standend();
  }
  if (ulmode) {
    ulmode = false;
    scr_underlineend();
  }
  if (bomode) {
    bomode = false;
    scr_boldend();
  }
  if (emph_mode) {
    emph_mode = false;
    scr_boldend();
  }

  if (anch_mode) {
    anch_mode = false;
    scr_underlineend();
  }
  if (imag_mode) {
    imag_mode = false;
    scr_standend();
  }
  if (form_mode) {
    form_mode = false;
    scr_standend();
  }
  if (visited_mode) {
    visited_mode = false;
  }
  if (active_mode) {
    active_mode = false;
    scr_boldend();
  }
  if (mark_mode) {
    mark_mode = false;
    scr_standend();
  }
  if (graph_mode) {
    graph_mode = false;
    scr_graphend();
  }
}

static void do_effect1(Lineprop m, enum CharEffects effect, bool *modeflag,
                       void (*action_start)(), void (*action_end)()) {
  if (m & effect) {
    if (!*modeflag) {
      if (action_start) {
        action_start();
      }
      *modeflag = true;
    }
  }
}

static void do_effect2(Lineprop m, enum CharEffects effect, bool *modeflag,
                       void (*action_start)(), void (*action_end)()) {
  if (*modeflag) {
    if (action_end) {
      action_end();
    }
    *modeflag = false;
  }
}

static void do_effects(Lineprop m) {
  /* effect end */
  do_effect2(m, PE_UNDER, &ulmode, scr_underline, scr_underlineend);
  do_effect2(m, PE_STAND, &somode, scr_standout, scr_standend);
  do_effect2(m, PE_BOLD, &bomode, scr_bold, scr_boldend);
  do_effect2(m, PE_EMPH, &emph_mode, scr_bold, scr_boldend);
  do_effect2(m, PE_ANCHOR, &anch_mode, scr_underline, scr_underlineend);
  do_effect2(m, PE_IMAGE, &imag_mode, scr_standout, scr_standend);
  do_effect2(m, PE_FORM, &form_mode, scr_standout, scr_standend);
  do_effect2(m, PE_VISITED, &visited_mode, nullptr, nullptr);
  do_effect2(m, PE_ACTIVE, &active_mode, scr_bold, scr_boldend);
  do_effect2(m, PE_MARK, &mark_mode, scr_standout, scr_standend);
  if (graph_mode) {
    scr_graphend();
    graph_mode = false;
  }

  /* effect start */
  do_effect1(m, PE_UNDER, &ulmode, scr_underline, scr_underlineend);
  do_effect1(m, PE_STAND, &somode, scr_standout, scr_standend);
  do_effect1(m, PE_BOLD, &bomode, scr_bold, scr_boldend);
  do_effect1(m, PE_EMPH, &emph_mode, scr_bold, scr_boldend);
  do_effect1(m, PE_ANCHOR, &anch_mode, scr_underline, scr_underlineend);
  do_effect1(m, PE_IMAGE, &imag_mode, scr_standout, scr_standend);
  do_effect1(m, PE_FORM, &form_mode, scr_standout, scr_standend);
  do_effect1(m, PE_VISITED, &visited_mode, nullptr, nullptr);
  do_effect1(m, PE_ACTIVE, &active_mode, scr_bold, scr_boldend);
  do_effect1(m, PE_MARK, &mark_mode, scr_standout, scr_standend);
}

struct Line *lineSkip(struct Viewport *viewport, struct Line *line,
                      struct Line *lastLine, int offset, int last) {
  auto l = currentLineSkip(line, offset, last);
  if (!nextpage_topline) {
    for (int i = viewport->LINES - 1 - (lastLine->linenumber - l->linenumber);
         i > 0 && l->prev != NULL; i--, l = l->prev)
      ;
  }
  return l;
}

void addMChar(const uint8_t *p, Lineprop mode, size_t len) {
  Lineprop m = CharEffect(mode);
  char c = *p;

  if (mode & PC_WCHAR2)
    return;

  do_effects(m);
  if (mode & PC_SYMBOL) {
    char **symbol;
    int w = (mode & PC_KANJI) ? 2 : 1;

    // c = ((char)wtf_get_code((wc_uchar *)p) & 0x7f) - SYMBOL_BASE;
    // if (term_graph_ok() && c < N_GRAPH_SYMBOL) {
    //   if (!graph_mode) {
    //     scr_graphstart();
    //     graph_mode = true;
    //   }
    //   // if (w == 2 && WcOption.use_wide)
    //   //   addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
    //   // else
    //   scr_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
    // } else
    {
      symbol = get_symbol();
      scr_addstr(symbol[(unsigned char)c % N_SYMBOL]);
    }
  } else if (mode & PC_CTRL) {
    switch (c) {
    case '\t':
      scr_addch(c);
      break;
    case '\n':
      scr_addch(' ');
      break;
    case '\r':
      break;
    case DEL_CODE:
      scr_addstr("^?");
      break;
    default:
      scr_addch('^');
      scr_addch(c + '@');
      break;
    }
  }
  // else if (mode & PC_UNKNOWN) {
  //   char buf[5];
  //   sprintf(buf, "[%.2X]", (unsigned char)wtf_get_code((wc_uchar *)p) |
  //   0x80); scr_addstr(buf);
  // }
  else {
    scr_addutf8(p);
  }
}

void addChar(char c, Lineprop mode) { addMChar((const uint8_t *)&c, mode, 1); }

struct Line *render_line(struct Line *l, const struct Viewport *viewport,
                         int i) {

  scr_move(i, viewport->rootX);
  if (l->width < 0)
    l->width = COLPOS(l, l->len);

  int column = viewport->currentColumn;
  if (l->len == 0 || l->width - 1 < column) {
    scr_clrtoeolx();
    return l;
  }

  /* need_clrtoeol(); */
  int pos = columnPos(l, column);
  auto p = &(l->lineBuf[pos]);
  auto pr = &(l->propBuf[pos]);
  int rcol = COLPOS(l, pos);
  int delta = 1;
  for (int j = 0; rcol - column < viewport->COLS && pos + j < l->len;
       j += delta) {
    delta = utf8sequence_len((const uint8_t *)&p[j]);
    if(delta==0){
      break;
    }
    int ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > viewport->COLS)
      break;
    if (rcol < column) {
      for (rcol = column; rcol < ncol; rcol++)
        addChar(' ', 0);
      continue;
    }
    if (p[j] == '\t') {
      for (; rcol < ncol; rcol++)
        addChar(' ', 0);
    } else {
      addMChar((const uint8_t *)&p[j], pr[j], delta);
    }
    rcol = ncol;
  }
  clear_effects();
  if (rcol - column < viewport->COLS)
    scr_clrtoeolx();
  return l;
}

void render_line_region(struct Viewport *viewport, struct Line *l, int i,
                        int bpos, int epos) {
  if (l == NULL)
    return;

  int column = viewport->currentColumn;
  int pos = columnPos(l, column);
  auto p = &(l->lineBuf[pos]);
  auto pr = &(l->propBuf[pos]);
  int rcol = COLPOS(l, pos);
  int delta = 1;
  int bcol = bpos - pos;
  int ecol = epos - pos;
  for (int j = 0; rcol - column < viewport->COLS && pos + j < l->len;
       j += delta) {
    delta = utf8sequence_len((const uint8_t *)&p[j]);
    int ncol = COLPOS(l, pos + j + delta);
    if (ncol - column > viewport->COLS)
      break;
    if (j >= bcol && j < ecol) {
      if (rcol < column) {
        // scr_move(i, viewport->rootX);
        for (rcol = column; rcol < ncol; rcol++)
          addChar(' ', 0);
        continue;
      }
      // scr_move(i, rcol - column + viewport->rootX);
      if (p[j] == '\t') {
        for (; rcol < ncol; rcol++)
          addChar(' ', 0);
      } else {
        addMChar((const uint8_t *)&p[j], pr[j], delta);
      }
    }
    rcol = ncol;
  }
  clear_effects();
}
