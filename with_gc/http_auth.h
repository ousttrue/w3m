#pragma once

struct Str;
struct auth_param {
  const char *name;
  Str *val;
};

struct HttpRequest;
struct Form;
struct Url;
using HttpAuthFunc = Str *(*)(struct http_auth *ha, Str *uname, Str *pw,
                              const Url &pu, HttpRequest *hr,
                              Form *request);
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
void getAuthCookie(struct http_auth *hauth, const char *auth_header,
                   const Url &pu, HttpRequest *hr, Form *request,
                   Str **uname, Str **pwd);
