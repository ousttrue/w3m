#pragma once

#define NO_REFERER ((const char *)-1)

struct HttpOption {
  const char *referer = {};
  bool no_cache = {};
};
