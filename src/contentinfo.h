#pragma once
#include "url.h"
#include "optional"

struct TextList;
struct ContentInfo {
  Url currentURL = {.schema = SCM_UNKNOWN};
  std::optional<Url> baseURL;
  const char *type = nullptr;
  const char *real_type = nullptr;
  TextList *document_header = nullptr;
  const char *filename = nullptr;
};
