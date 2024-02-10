#pragma once
#include "url.h"
#include "url_stream.h"
#include "optional"
#include <vector>
#include <memory>

extern int FollowRedirection;

struct TextList;
struct HttpResponse {
  int http_response_code = 0;
  Url currentURL = {.schema = SCM_UNKNOWN};
  std::optional<Url> baseURL;
  const char *type = "text/plain";
  const char *real_type = nullptr;
  UrlSchema real_schema = {};
  TextList *document_header = nullptr;
  const char *filename = nullptr;
  std::string sourcefile;
  std::vector<Url> redirectins;
  const char *ssl_certificate = nullptr;
  const char *baseTarget = nullptr;
  size_t trbyte = 0;
  const char *edit = nullptr;
  long long current_content_length;
  UrlStream f;

  HttpResponse();
  ~HttpResponse();
  bool checkRedirection(const Url &pu);
  int readHeader(struct UrlStream *uf, const Url &pu);
  const char *getHeader(const char *field) const;
  const char *checkContentType() const;
  const char *guess_save_name(const char *file) const;
};

const char *mybasename(const char *s);
const char *guess_filename(const char *file);
