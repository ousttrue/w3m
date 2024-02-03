#pragma once
#include <stdint.h>
// #include "enum_template.h"

struct Str;
struct Url;
struct TextList;

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

extern bool override_user_agent;
extern char *UserAgent;
extern const char *AcceptLang;
extern char *AcceptEncoding;
extern char *AcceptMedia;
extern bool NoCache;
extern bool NoSendReferer;
extern bool CrossOriginReferer;
extern bool override_content_type;
extern Str *header_string;

struct FormList;

enum HttpMethod  {
  HR_COMMAND_GET = 0,
  HR_COMMAND_POST = 1,
  HR_COMMAND_CONNECT = 2,
  HR_COMMAND_HEAD = 3,
};

enum HttpRequestFlags  {
  HR_FLAG_LOCAL = 1,
  HR_FLAG_PROXY = 2,
};
// ENUM_OP_INSTANCE(HttpRequestFlags);

struct HRequest {
  HttpMethod method;
  HttpRequestFlags flag = {};
  const char *referer;
  FormList *request;
};

Str *HTTPrequest(Url *pu, Url *current, HRequest *hr, TextList *extra);
Str *HTTPrequestMethod(HRequest *hr);
Str *HTTPrequestURI(Url *pu, HRequest *hr);
