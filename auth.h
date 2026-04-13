#pragma once
#include "Str.h"

struct auth_pass {
    int bad;
    int is_proxy;
    Str host;
    int port;
    /*    Str file; */
    Str realm;
    Str uname;
    Str pwd;
    struct auth_pass* next;
};
struct Url;

FILE* openSecretFile(const char* fname);
void loadPasswd(void);
void add_auth_user_passwd(struct Url* pu, const char* realm, Str uname, Str pwd, int is_proxy);
int find_auth_user_passwd(struct Url* pu, const char* realm, Str* uname, Str* pwd, int is_proxy);
void invalidate_auth_user_passwd(struct Url* pu, char* realm, Str uname, Str pwd, int is_proxy);
