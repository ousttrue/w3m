#pragma once
#include "http_option.h"
#include "bufferpoint.h"

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

  int onAnchor(const BufferPoint &bp) const;
};
