#pragma once
#include <vector>

struct BufferPoint {
  int line = 0;
  int pos = 0;
  int invalid = 0;

  int cmp(const BufferPoint &b) const {
    return (this->line - b.line) ? (this->line - b.line) : (this->pos - b.pos);
  }
};

struct HmarkerList {
private:
  std::vector<BufferPoint> marks;
  int prevhseq = -1;

public:
  const BufferPoint *get(int hseq) const {
    if (hseq >= 0 && hseq < (int)this->size()) {
      return &this->marks[hseq];
    } else {
      return nullptr;
    }
  }
  BufferPoint *get(int hseq) {
    if (hseq >= 0 && hseq < (int)this->size()) {
      return &this->marks[hseq];
    } else {
      return nullptr;
    }
  }
  size_t size() const { return marks.size(); }
  void clear() { marks.clear(); }
  void putHmarker(int line, int pos, int seq);
  void invalidate(int i) { this->marks[i].invalid = 1; }
  void set(int i, int l, int pos) {
    this->marks[i].line = l;
    this->marks[i].pos = pos;
  }
  bool tryMark(int h, int nlines, int pos) {
    if (h < (int)this->size() && this->marks[h].invalid) {
      this->marks[h].pos = pos;
      this->marks[h].line = nlines;
      this->marks[h].invalid = 0;
      return true;
    }

    return false;
  }
};
