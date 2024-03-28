#pragma once
#include <memory>
#include <string>

struct Url;
struct HttpRequest;
struct Form;
std::string AuthDigestCred(struct http_auth *ha, const std::string &uname,
                           const std::string &pw, const Url &pu,
                           HttpRequest *hr,
                           const std::shared_ptr<Form> &request);
