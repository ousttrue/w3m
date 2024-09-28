#pragma once
#include "Str.h"

struct auth_param {
  char *name;
  Str val;
};

struct Url;
struct HttpRequest;
struct http_auth;
struct FormList;
typedef Str (*CredFunc)(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
                        struct HttpRequest *hr, struct FormList *request);
struct http_auth {
  int pri;
  char *scheme;
  struct auth_param *param;
  CredFunc cred;
};

extern struct auth_param digest_auth_param[8];

Str qstr_unquote(Str s);
Str get_auth_param(struct auth_param *auth, char *name);
Str AuthDigestCred(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
                   struct HttpRequest *hr, struct FormList *request);
