#pragma once
#include "fm.h"

struct auth_param {
  char *name;
  Str val;
};

struct http_auth {
  int pri;
  char *scheme;
  struct auth_param *param;
  Str (*cred)(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
              struct HttpRequest *hr, struct FormList *request);
};

extern struct auth_param digest_auth_param[8];

Str qstr_unquote(Str s);
Str get_auth_param(struct auth_param *auth, char *name);
Str AuthDigestCred(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
                   struct HttpRequest *hr, struct FormList *request);
