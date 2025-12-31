#pragma once
#include "http_request.h"

struct auth_param {
    const char* name;
    Str val;
};

struct Url;
struct http_auth {
    int pri;
    const char* scheme;
    struct auth_param* param;
    Str (*cred)(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
        struct HttpRequest* hr, struct FormList* request);
};

struct http_auth*
findAuthentication(struct http_auth* hauth, struct TextList* document_header, char* auth_field);

Str get_auth_param(struct auth_param* auth, const char* name);

void getAuthCookie(struct http_auth* hauth, const char* auth_header,
    struct TextList* extra_header, struct Url* pu, struct HttpRequest* hr,
    struct FormList* request,
    Str* uname, Str* pwd);
