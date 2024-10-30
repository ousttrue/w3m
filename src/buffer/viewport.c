#include "buffer/viewport.h"
#include "buffer/line.h"
#include "term/scr.h"
#include "text/ctrlcode.h"
#include "text/symbol.h"

bool nextpage_topline = false;

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
    if (delta == 0) {
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
