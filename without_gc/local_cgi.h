#pragma once
#include <stdio.h>
#include <string>
#include <memory>

extern std::string document_root;
extern std::string cgi_bin;

void set_environ(const char *var, const char *value);
std::string localCookie();

struct Form;
struct HttpOption;
FILE *localcgi_post(const char *uri, const char *qstr,
                    const std::shared_ptr<Form> &request,
                    const HttpOption &option);

inline FILE *localcgi_get(const char *u, const char *q, const HttpOption &r) {
  return localcgi_post((u), (q), NULL, (r));
}
