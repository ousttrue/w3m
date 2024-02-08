#pragma once
#include "enum_template.h"
#include <string>
#include <optional>

struct Str;
struct Url;
struct TextList;
struct FormList;

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

enum HttpStreamStatus {
  HTST_UNKNOWN = 255,
  HTST_MISSING = 254,
  HTST_NORMAL = 0,
  HTST_CONNECT = 1,
};

enum class HttpMethod {
  GET = 0,
  POST = 1,
  CONNECT = 2,
  HEAD = 3,
};
inline std::string to_str(HttpMethod method) {
  switch (method) {
  case HttpMethod::CONNECT:
    return "CONNECT";
  case HttpMethod::POST:
    return "POST";
  case HttpMethod::HEAD:
    return "HEAD";
  case HttpMethod::GET:
  default:
    return "GET";
  }
}

enum HttpRequestFlags {
  HR_FLAG_LOCAL = 1,
  HR_FLAG_PROXY = 2,
};
ENUM_OP_INSTANCE(HttpRequestFlags);

#define NO_REFERER ((const char *)-1)

struct HttpOption {
  const char *referer = {};
  bool no_cache = {};
};

struct HttpRequest {
  HttpStreamStatus status = {};
  HttpMethod method = {};
  HttpRequestFlags flag = {};
  HttpOption option = {};
  FormList *request = {};

  Str *uname = NULL;
  Str *pwd = NULL;
  Str *realm = NULL;
  bool add_auth_cookie_flag = false;

  TextList *extra_headers = {};

  HttpRequest(const HttpOption option, FormList *request)
      : option(option), request(request) {}

  Str *getRequestURI(const Url &url) const;
  Str *to_Str(const Url &pu, std::optional<Url> current) const;
  void add_auth_cookie(const Url &pu);
};
