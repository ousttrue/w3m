#pragma once
#include <stdio.h>
#include <string>

extern std::string passwd_file;
extern bool disable_secret_security_check;

struct Url;
struct auth_pass {
  bool bad = false;
  bool is_proxy = false;
  std::string host;
  int port = {};
  std::string realm = {};
  std::string uname = {};
  std::string pwd = {};
};

void add_auth_user_passwd(const Url &pu, const std::string &realm,
                          const std::string &uname, const std::string &pwd,
                          bool is_proxy);

void invalidate_auth_user_passwd(const Url &pu, const std::string &realm,
                                 bool is_proxy);
std::pair<std::string, std::string>
find_auth_user_passwd(const Url &pu, const std::string &realm, bool is_proxy);
void loadPasswd();
