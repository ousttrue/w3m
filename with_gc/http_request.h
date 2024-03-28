#pragma once
#include "url.h"
#include "http_option.h"
#include "enum_template.h"
#include <string>
#include <list>
#include <optional>
#include <memory>

extern bool override_user_agent;
extern std::string UserAgent;
extern std::string AcceptLang;
extern std::string AcceptEncoding;
extern std::string AcceptMedia;
extern bool NoCache;
extern bool NoSendReferer;
extern bool CrossOriginReferer;
extern bool override_content_type;
extern std::string header_string;

enum HttpStreamStatus {
  HTST_NORMAL = 0,
  HTST_CONNECT = 1,
  HTST_MISSING = 254,
  HTST_UNKNOWN = 255,
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

struct Url;
struct Form;
struct HttpRequest {
  Url url;
  std::optional<Url> current;
  HttpStreamStatus status = {};
  HttpMethod method = {};
  HttpRequestFlags flag = {};
  HttpOption option = {};
  std::shared_ptr<Form> request = {};

  std::string uname;
  std::string pwd;
  std::string realm;
  bool add_auth_cookie_flag = false;

  std::list<std::string> extra_headers;

  HttpRequest(const Url &url, std::optional<Url> &current,
              const HttpOption option, const std::shared_ptr<Form> &request)
      : url(url), current(current), option(option), request(request) {}
  std::string getRequestURI() const;
  std::string to_Str() const;
  void add_auth_cookie();
};
