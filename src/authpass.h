#pragma once
#include <stdio.h>

extern const char *passwd_file;
extern bool disable_secret_security_check;

struct Str;
struct Url;
struct auth_pass {
  int bad = 0;
  bool is_proxy = {};
  Str *host = {};
  int port = {};
  Str *realm = {};
  Str *uname = {};
  Str *pwd = {};
  auth_pass *next = {};
};

void add_auth_user_passwd(Url *pu, const char *realm, Str *uname, Str *pwd,
                          bool is_proxy);

void invalidate_auth_user_passwd(Url *pu, const char *realm, Str *uname,
                                 Str *pwd, bool is_proxy);
int find_auth_user_passwd(Url *pu, const char *realm, Str **uname, Str **pwd,
                          bool is_proxy);
void loadPasswd();

FILE *openSecretFile(const char *fname);
