#pragma once
#include "url.h"
#include <string>
#include <list>
#include <optional>

extern bool default_use_cookie;
extern bool use_cookie;
extern bool show_cookie;
extern bool accept_cookie;
extern int accept_bad_cookie;
extern bool no_rc_dir;

enum AcceptBadCookieMode {
  ACCEPT_BAD_COOKIE_DISCARD = 0,
  ACCEPT_BAD_COOKIE_ACCEPT = 1,
  ACCEPT_BAD_COOKIE_ASK = 2,
};

enum CookieFlags {
  COO_NONE = 0,
  COO_USE = 1,
  COO_SECURE = 2,
  COO_DOMAIN = 4,
  COO_PATH = 8,
  COO_DISCARD = 16,
  COO_EPORT = 9, /* Port match failed (version 1' case 5) */
  COO_EMAX = COO_EPORT,
  COO_OVERRIDE = 32,    /* user chose to override security checks */
  COO_OVERRIDE_OK = 32, /* flag to specify that an error is overridable */
};

std::optional<std::string> find_cookie(const Url &pu);

void save_cookies(const std::string &cookie_file);
void load_cookies(const std::string &cookie_file);
bool check_cookie_accept_domain(const char *domain);
int add_cookie(const Url *pu, std::string_view name, std::string_view value,
               time_t expires, std::string_view domain, std::string_view path,
               CookieFlags flag, std::string_view comment, int version,
               std::string_view port, std::string_view commentURL);

std::string cookie_list_panel();
void set_cookie_flag(struct keyvalue *arg);
