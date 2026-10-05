#pragma once
#include "url.h"
#include "Str.h"

struct auth_param {
    char* name;
    pStr val;
};

struct HttpRequest;
struct form_list;
struct http_auth {
    int pri;
    char* scheme;
    struct auth_param* param;
    pStr (*cred)(struct http_auth* ha, pStr uname, pStr pw, struct Url* pu,
        struct HttpRequest* hr, struct form_list* request);
};

enum AUTHCHR {
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

// from ftp
int find_auth_user_passwd(struct Url* pu, char* realm,
    pStr* uname, pStr* pwd, int is_proxy);

// from ftp and http
void add_auth_user_passwd(struct Url* pu, char* realm,
    pStr uname, pStr pwd, int is_proxy);

// from file
struct TextList;
void getAuthCookie(struct http_auth* hauth, char* auth_header,
    struct TextList* extra_header, struct Url* pu, struct HttpRequest* hr,
    struct form_list* request,
    pStr* uname, pStr* pwd);

// from file
struct _Buffer;
struct http_auth*
findAuthentication(struct http_auth* hauth, struct _Buffer* buf, char* auth_field);
pStr get_auth_param(struct auth_param* auth, const char* name);
