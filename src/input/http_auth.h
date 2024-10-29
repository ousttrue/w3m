#pragma once
#include "text/Str.h"

extern bool disable_secret_security_check;
extern const char *passwd_file;

struct Url;
struct HttpRequest;
struct http_auth;
struct HttpResponse;

struct auth_param {
  const char *name;
  Str val;
};

typedef Str (*CredFunc)(struct http_auth *ha, struct HttpRequest *hr, Str uname,
                        Str pw);

struct http_auth {
  int pri;
  char *scheme;
  struct auth_param *param;
  CredFunc cred;
};

struct auth_pass {
  int bad;
  int is_proxy;
  Str host;
  int port;
  /*    Str file; */
  Str realm;
  Str uname;
  Str pwd;
  struct auth_pass *next;
};

void invalidate_auth_user_passwd(struct Url *pu, char *realm, Str uname,
                                 Str pwd, int is_proxy);
Str qstr_unquote(Str s);
Str get_auth_param(struct auth_param *auth, char *name);
void getAuthCookie(struct http_auth *hauth, char *auth_header,
                   struct HttpRequest *hr, Str *uname, Str *pwd);
struct http_auth *findAuthentication(struct http_auth *hauth,
                                     struct HttpResponse *http_response,
                                     char *auth_field);

void add_auth_pass_entry(const struct auth_pass *ent, int netrc, int override);
FILE *openSecretFile(const char *fname);
int find_auth_user_passwd(struct Url *pu, char *realm, Str *uname, Str *pwd,
                          int is_proxy);
void add_auth_user_passwd(struct Url *pu, char *realm, Str uname, Str pwd,
                          int is_proxy);
void loadPasswd(void);
