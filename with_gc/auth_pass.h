#pragma once
#include <stdio.h>
#include <string>

extern std::string passwd_file;
extern bool disable_secret_security_check;

struct Str;
struct Url;
struct auth_pass {
  bool bad = false;
  bool is_proxy = {};
  Str *host = {};
  int port = {};
  Str *realm = {};
  Str *uname = {};
  Str *pwd = {};
  auth_pass *next = {};
};

void add_auth_user_passwd(const Url &pu, const char *realm, Str *uname,
                          Str *pwd, bool is_proxy);

void invalidate_auth_user_passwd(const Url &pu, const char *realm, Str *uname,
                                 Str *pwd, bool is_proxy);
int find_auth_user_passwd(const Url &pu, const char *realm, Str **uname,
                          Str **pwd, bool is_proxy);
void loadPasswd();

FILE *openSecretFile(const char *fname);
