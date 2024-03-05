#pragma once
#include "http_option.h"
#include <vector>
#include <string_view>

#define bpcmp(a, b)                                                            \
  (((a).line - (b).line) ? ((a).line - (b).line) : ((a).pos - (b).pos))

extern bool MarkAllPages;
extern int PagerMax;

struct BufferPoint {
  int line = 0;
  int pos = 0;
  int invalid = 0;
};

struct Anchor {
  const char *url = nullptr;
  std::string target;
  HttpOption option = {};
  const char *title = nullptr;
  unsigned char accesskey = 0;
  BufferPoint start = {};
  BufferPoint end = {};
  int hseq = 0;
  char slave = 0;
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
