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
extern std::string cookie_reject_domains;
extern std::string cookie_accept_domains;
extern std::string cookie_avoid_wrong_number_of_dots;
extern std::list<std::string> Cookie_reject_domains;
extern std::list<std::string> Cookie_accept_domains;
extern std::list<std::string> Cookie_avoid_wrong_number_of_dots_domains;
extern int DNS_order;

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
  COO_OVERRIDE = 32, /* user chose to override security checks */
};

std::optional<std::string> find_cookie(const Url &pu);

void save_cookies();
void load_cookies();
void initCookie();
bool check_cookie_accept_domain(const char *domain);
void process_http_cookie(const Url *pu, std::string_view lineBuf2);
std::string cookie_list_panel();
