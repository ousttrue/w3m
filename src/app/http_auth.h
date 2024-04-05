#pragma once
#include <memory>
#include <string>
#include <optional>

struct HttpRequest;
struct Form;
struct Url;
using HttpAuthFunc = std::string (*)(struct http_auth *ha,
                                     const std::string &uname,
                                     const std::string &pw, const Url &pu,
                                     HttpRequest *hr,
                                     const std::shared_ptr<Form> &form);

struct HttpResponse;
struct http_auth {
  int pri = 0;
  std::string schema;
  struct auth_param *param = nullptr;
  HttpAuthFunc cred = {};
  std::string get_auth_param(const char *name);
};

std::optional<http_auth> findAuthentication(const HttpResponse &res,
                                            const char *auth_field);
std::tuple<std::string, std::string>
getAuthCookie(struct http_auth *hauth, const char *auth_header, const Url &pu,
              HttpRequest *hr, const std::shared_ptr<Form> &form);
