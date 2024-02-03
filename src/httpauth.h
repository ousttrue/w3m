#pragma once

struct Str;
struct auth_param {
  const char *name;
  Str *val;
};

struct HRequest;
struct FormList;
struct Url;
using HttpAuthFunc = Str *(*)(struct http_auth *ha, Str *uname, Str *pw,
                              Url *pu, HRequest *hr, FormList *request);
struct http_auth {
  int pri;
  const char *schema;
  struct auth_param *param;
  HttpAuthFunc cred;
};

struct Buffer;
http_auth *findAuthentication(http_auth *hauth, Buffer *buf,
                              const char *auth_field);
Str *get_auth_param(auth_param *auth, const char *name);
Str *qstr_unquote(Str *s);
struct TextList;
void getAuthCookie(struct http_auth *hauth, const char *auth_header,
                   TextList *extra_header, Url *pu, HRequest *hr,
                   FormList *request, Str **uname, Str **pwd);
