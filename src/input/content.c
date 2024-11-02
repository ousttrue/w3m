#include "content.h"
#include "alloc.h"
#include <string.h>

struct Content *newContent(struct Url url, const char *bytes,
                           size_t byte_length, const char *content_type,
                           enum CharSet charset) {
  auto content = New(struct Content);
  memset(content, 0, sizeof(struct Content));
  content->url = url;
  // content->url.scheme = SCM_UNKNOWN;
  content->content = bytes;
  content->content_length = byte_length;
  content->content_type = content_type;
  content->charset = charset;
  return content;
}
