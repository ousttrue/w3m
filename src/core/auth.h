#pragma once
#include <Str.h>
#include "textlist.h"

struct _ParsedURL;
struct HttpRequest;
struct form_list;

enum {
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

struct auth_param {
    const char* name;
    Str val;
};
Str get_auth_param(struct auth_param* auth, const char* name);

struct http_auth {
    int pri;
    const char* scheme;
    struct auth_param* param;
    Str (*cred)(struct http_auth* ha, Str uname, Str pw, struct _ParsedURL* pu,
        struct HttpRequest* hr, struct form_list* request);
};
void getAuthCookie(struct http_auth* hauth, const char* auth_header,
    TextList* extra_header, struct _ParsedURL* pu, struct HttpRequest* hr,
    struct form_list* request,
    volatile Str* uname, volatile Str* pwd);

Str AuthDigestCred(struct http_auth* ha, Str uname, Str pw,
    struct _ParsedURL* pu, struct HttpRequest* hr, struct form_list* request);
Str qstr_unquote(Str s);
struct http_auth* findAuthentication(struct http_auth* hauth, TextList* document_header, const char* auth_field);
