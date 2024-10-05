#pragma once
#include "http_auth.h"

extern struct auth_param digest_auth_param[8];

Str AuthDigestCred(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
                   struct HttpRequest *hr, struct FormList *request);
