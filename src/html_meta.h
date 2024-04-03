#pragma once
#include <string>

struct MetaRefreshInfo {
  int interval = 0;
  std::string url;
};
MetaRefreshInfo getMetaRefreshParam(const std::string &q);
