#pragma once
#include "url.h"
#include "Str.h"
#include "textlist.h"

struct auth_param {
    char* name;
    pStr val;
};

struct http_request;
struct form_list;
struct http_auth {
    int pri;
    char* scheme;
    struct auth_param* param;
    pStr (*cred)(struct http_auth* ha, pStr uname, pStr pw, ParsedURL* pu,
        struct http_request* hr, struct form_list* request);
};

enum AUTHCHR {
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

// from ftp
int find_auth_user_passwd(ParsedURL* pu, char* realm,
    pStr* uname, pStr* pwd, int is_proxy);

// from ftp and http
void add_auth_user_passwd(ParsedURL* pu, char* realm,
    pStr uname, pStr pwd, int is_proxy);

// from file
void getAuthCookie(struct http_auth* hauth, char* auth_header,
    TextList* extra_header, ParsedURL* pu, struct http_request* hr,
    struct form_list* request,
    pStr* uname, pStr* pwd);

// from file
struct _Buffer;
struct http_auth*
findAuthentication(struct http_auth* hauth, struct _Buffer* buf, char* auth_field);
pStr get_auth_param(struct auth_param* auth, const char* name);
