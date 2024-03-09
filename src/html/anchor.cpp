#include "anchor.h"
#include "readbuffer.h"
#include "line_layout.h"
#include "Str.h"
#include "quote.h"
#include "entity.h"
#include "http_request.h"
#include "myctype.h"
#include "regex.h"
#include "alloc.h"

bool MarkAllPages = false;

#define PAGER_MAX_LINE 10000 /* Maximum line kept as pager */

#define FIRST_ANCHOR_SIZE 30

int Anchor::onAnchor(int line, int pos) {
  BufferPoint bp;
  bp.line = line;
  bp.pos = pos;
  if (bp.cmp(this->start) < 0) {
    return -1;
  }
  if (this->end.cmp(bp) <= 0) {
    return 1;
  }
  return 0;
}

#define FIRST_MARKER_SIZE 30
void HmarkerList::putHmarker(int line, int pos, int seq) {
  // if (seq + 1 > ml->nmark)
  //   ml->nmark = seq + 1;
  // if (ml->nmark >= ml->markmax) {
  //   ml->markmax = ml->nmark * 2;
  //   ml->marks = (BufferPoint *)New_Reuse(BufferPoint, ml->marks,
  //   ml->markmax);
  // }
  while (seq >= (int)marks.size()) {
    marks.push_back({});
  }
  this->marks[seq].line = line;
  this->marks[seq].pos = pos;
  this->marks[seq].invalid = 0;
}

const char *html_quote(const char *str) {
  Str *tmp = NULL;
  for (auto p = str; *p; p++) {
    auto q = html_quote_char(*p);
    if (q) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_charp(tmp, q);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return Strnew_charp(str)->ptr;
}
