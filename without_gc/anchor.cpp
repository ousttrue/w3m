#include "anchor.h"

int Anchor::onAnchor(const BufferPoint &bp) const {
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
