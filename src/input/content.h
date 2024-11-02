#pragma once
#include "input/charset.h"
#include "input/url.h"

struct Content {
  struct Url url;
  const char *content;
  size_t content_length;
  const char *content_type;
  enum CharSet charset;
  const char *sourcefile;
  enum URL_SCHEME_TYPE real_scheme;
  const char *filename;
};

struct Content *newContent(struct Url url, const char *content,
                           size_t content_length, const char *content_type,
                           enum CharSet charset);
