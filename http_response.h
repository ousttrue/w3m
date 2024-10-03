#pragma once
#include "Str.h"
#include <stdint.h>

struct Url;
struct URLFile;

struct HttpResponse {
  int http_status_code;
  struct TextList *document_header;
  int64_t current_content_length;
};

struct HttpResponse *httpReadHeader(struct URLFile *uf, struct Url *pu);
bool httpMatchattr(const char *p, const char *attr, int len, Str *value);
const char *httpGetHeader(struct HttpResponse *res, const char *field);
const char *httpGetContentType(struct HttpResponse *res);
