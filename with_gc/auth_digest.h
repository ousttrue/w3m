#pragma once
#include <memory>

struct Str;
struct Url;
struct HttpRequest;
struct Form;
Str *AuthDigestCred(struct http_auth *ha, Str *uname, Str *pw, const Url &pu,
                    HttpRequest *hr, const std::shared_ptr<Form> &request);
