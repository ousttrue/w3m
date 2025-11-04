#pragma once
#include "HttpRequest.h"

extern int QuietMessage;
extern int disable_secret_security_check;
extern const char* passwd_file;

enum {
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

struct auth_param {
    char* name;
    Str val;
};

struct http_auth {
    int pri;
    char* scheme;
    struct auth_param* param;
    Str (*cred)(struct http_auth* ha, Str uname, Str pw, struct Url* pu,
        struct HttpRequest* hr, struct form_list* request);
};

void getAuthCookie(struct http_auth* hauth, char* auth_header,
    TextList* extra_header, struct Url* pu, struct HttpRequest* hr,
    struct form_list* request,
    volatile Str* uname, volatile Str* pwd);
Str get_auth_param(struct auth_param* auth, char* name);
struct http_auth*
findAuthentication(struct http_auth* hauth, TextList* document_header, char* auth_field);
void add_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd, int is_proxy);
int find_auth_user_passwd(struct Url* pu, char* realm, Str* uname, Str* pwd, int is_proxy);
void loadPasswd(void);
FILE* openSecretFile(char* fname);
