#pragma once

struct Str;
struct Url;
struct HttpRequest;
struct FormList;
Str *AuthDigestCred(struct http_auth *ha, Str *uname, Str *pw, const Url &pu,
                    HttpRequest *hr, FormList *request);

