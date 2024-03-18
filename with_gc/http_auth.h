#pragma once
#include <memory>
#include <string>

struct Str;
struct auth_param {
  const char *name;
  Str *val;
};

struct HttpRequest;
struct Form;
struct Url;
using HttpAuthFunc = Str *(*)(struct http_auth *ha, const std::string &uname,
                              const std::string &pw, const Url &pu,
                              HttpRequest *hr,
                              const std::shared_ptr<Form> &form);
struct http_auth {
  int pri = 0;
  const char *schema = nullptr;
  struct auth_param *param = nullptr;
  HttpAuthFunc cred = {};
};

struct HttpResponse;
http_auth *findAuthentication(http_auth *hauth, const HttpResponse &res,
                              const char *auth_field);
Str *get_auth_param(auth_param *auth, const char *name);
Str *qstr_unquote(Str *s);
struct TextList;
std::tuple<std::string, std::string>
getAuthCookie(struct http_auth *hauth, const char *auth_header, const Url &pu,
              HttpRequest *hr, const std::shared_ptr<Form> &form);
