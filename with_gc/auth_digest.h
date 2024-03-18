#pragma once
#include <memory>
#include <string>

struct Str;
struct Url;
struct HttpRequest;
struct Form;
Str *AuthDigestCred(struct http_auth *ha, const std::string &uname,
                    const std::string &pw, const Url &pu, HttpRequest *hr,
                    const std::shared_ptr<Form> &request);
