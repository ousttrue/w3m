#pragma once
#include "Str.h"

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
  enum HttpRequestMethod command;
  enum HttpRequestFlags flag;
  const char *referer;
  struct FormList *request;
};

Str HTTPrequestMethod(struct HttpRequest *hr);
struct Url;
Str HTTPrequestURI(struct Url *pu, struct HttpRequest *hr);
