#pragma once
#include "input/istream.h"
#include "text/Str.h"
#include <stdint.h>

enum ContentType {
  CONTENTTYPE_TextPlane,
  CONTENTTYPE_TextHTml,
};

enum CharSet {
  CHARSET_UNKONWN,
  CHARSET_UTF8,
  // CHARSET_ISO_8859_1, 1byte subset of UTF8
  CHARSET_SJIS,
};

struct HttpResponse {
  struct Url url;

  enum StreamStatus stream_status;
  union input_stream *stream;

  int http_status_code;
  struct TextList *document_header;
  enum ContentType content_type;
  enum CharSet content_charset;
  int64_t content_length;
};

struct HttpResponse *newHttpResponse(struct Url url);
void httpReadResponse(struct HttpResponse *res);
bool httpMatchattr(const char *p, const char *attr, int len, Str *value);
const char *httpGetHeader(struct HttpResponse *res, const char *field);
const char *httpGetContentType(struct HttpResponse *res);
