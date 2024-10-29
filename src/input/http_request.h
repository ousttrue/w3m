#pragma once
#include "input/url.h"
#include "text/Str.h"

#define NO_REFERER ((char *)-1)

enum HttpRequestMethod {
  HR_COMMAND_GET = 0,
  HR_COMMAND_POST = 1,
  HR_COMMAND_CONNECT = 2,
  HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlags {
  HR_FLAG_LOCAL = 1,
  HR_FLAG_PROXY = 2,
};

struct HttpRequest {
  struct Url url;
  enum HttpRequestFlags flag;
  enum HttpRequestMethod command;
  struct FormList *form;
  struct TextList *extra_header;
  const char *referer;
  struct Url *current;
  bool no_cache;
};

struct HttpRequest *newHttpRequest(struct Url url, struct FormList *form,
                                   const char *referer, bool no_cache,
                                   struct TextList *extra_header);
Str HTTPrequestMethod(struct HttpRequest *hr);
struct Url;
Str HTTPrequestURI(struct HttpRequest *hr);
