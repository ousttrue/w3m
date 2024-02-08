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

enum HttpMethod {
  HR_COMMAND_GET = 0,
  HR_COMMAND_POST = 1,
  HR_COMMAND_CONNECT = 2,
  HR_COMMAND_HEAD = 3,
};

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
  std::string getMehodString() const {
    switch (method) {
    case HR_COMMAND_CONNECT:
      return "CONNECT";
    case HR_COMMAND_POST:
      return "POST";
    case HR_COMMAND_HEAD:
      return "HEAD";
    case HR_COMMAND_GET:
    default:
      return "GET";
    }
  }
  HttpRequestFlags flag = {};
  HttpOption option = {};
  FormList *request = {};

  HttpRequest(const HttpOption option, FormList *request)
      : option(option), request(request) {}

  Str *getRequestURI(const Url &url) const;
  Str *to_Str(const Url &pu, std::optional<Url> current, TextList *extra) const;
};
