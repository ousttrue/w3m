#pragma once

extern const char *passwd_file;
extern bool disable_secret_security_check;

struct Str;
struct auth_param {
  const char *name;
  Str *val;
};

struct HRequest;
struct FormList;
struct ParsedURL;
using HttpAuthFunc = Str *(*)(struct http_auth *ha, Str *uname, Str *pw,
                              ParsedURL *pu, HRequest *hr, FormList *request);
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
                   TextList *extra_header, ParsedURL *pu, HRequest *hr,
                   FormList *request, Str **uname, Str **pwd);
