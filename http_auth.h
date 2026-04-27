#pragma once
#include "Str.h"

struct auth_param {
    const char* name;
    Str val;
};

struct Url;
struct http_auth;
struct HttpRequest;
struct HttpResponse;
struct Form;
typedef Str (*CredFunc)(struct http_auth* ha, Str uname, Str pw, struct Url* pu, struct HttpRequest* req, struct Form* post);

struct http_auth {
    int pri;
    const char* scheme;
    struct auth_param* param;
    CredFunc cred;
};

struct http_auth* findAuthentication(struct HttpResponse* res, struct http_auth* hauth, const char* auth_field);
Str get_auth_param(struct auth_param* auth, const char* name);
struct CmdArgs;
struct _textlist;
void getAuthCookie(struct CmdArgs* args, struct http_auth* hauth,
    const char* auth_header,
    struct _textlist* extra_header, struct Url* pu, struct HttpRequest* hr,
    struct Form* request,
    volatile Str* uname, volatile Str* pwd);
