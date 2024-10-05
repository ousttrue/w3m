#pragma once
#include "text/Str.h"
#include <stdint.h>

struct Url;
struct URLFile;

enum ContentType {
  CONTENTTYPE_TextPlane,
  CONTENTTYPE_TextHTml,
};
enum CharSet {
  CHARSET_ISO_8859_1,
  CHARSET_SJIS,
};
struct HttpResponse {
  int http_status_code;
  struct TextList *document_header;
  enum ContentType content_type;
  enum CharSet content_charset;
  int64_t content_length;
};

struct HttpResponse *httpReadResponse(struct URLFile *uf, struct Url *pu);
bool httpMatchattr(const char *p, const char *attr, int len, Str *value);
const char *httpGetHeader(struct HttpResponse *res, const char *field);
const char *httpGetContentType(struct HttpResponse *res);

