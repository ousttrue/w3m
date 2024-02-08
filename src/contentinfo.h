#pragma once
#include "url.h"
#include "optional"

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
  struct MailcapEntry *mailcap = nullptr;
  const char *mailcap_source = nullptr;

  ~ContentInfo();
};
