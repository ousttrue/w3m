#include "viewport.h"
#include "line.h"

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
