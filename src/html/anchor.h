#pragma once
#include "http_option.h"
#include <vector>

extern bool MarkAllPages;

struct BufferPoint {
  int line = 0;
  int pos = 0;
  int invalid = 0;

  int cmp(const BufferPoint &b) const {
    return (this->line - b.line) ? (this->line - b.line) : (this->pos - b.pos);
  }
};

struct Anchor {
  std::string url;
  std::string target;
  HttpOption option = {};
  std::string title;
  unsigned char accesskey = 0;
  BufferPoint start = {};
  BufferPoint end = {};
  int hseq = 0;
  bool slave = false;
  short y = 0;
  short rows = 0;

  int onAnchor(int line, int pos);
};

struct HmarkerList {
  std::vector<BufferPoint> marks;
  size_t size() const { return marks.size(); }
  void clear() { marks.clear(); }
  int prevhseq = -1;
  void putHmarker(int line, int pos, int seq);
};
