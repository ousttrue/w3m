#pragma once
#include "url.h"
#include "optional"
#include <vector>

extern int FollowRedirection;

struct TextList;
struct ContentInfo {
  Url currentURL = {.schema = SCM_UNKNOWN};
  std::optional<Url> baseURL;
  const char *type = nullptr;
  const char *real_type = nullptr;
  UrlSchema real_schema = {};
  TextList *document_header = nullptr;
  const char *filename = nullptr;
  std::string sourcefile;
  std::vector<Url> redirectins;

  ~ContentInfo();
  bool checkRedirection(const Url &pu);
};
