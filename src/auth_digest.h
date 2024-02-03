#pragma once

struct Str;
struct Url;
struct HRequest;
struct FormList;
Str *AuthDigestCred(struct http_auth *ha, Str *uname, Str *pw, Url *pu,
                    HRequest *hr, FormList *request);

