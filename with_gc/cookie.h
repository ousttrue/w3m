#pragma once
#include "url.h"
#include <time.h>
#include <gc_cpp.h>
#include <string>
#include <list>

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

struct Cookie : public gc_cleanup {
  Url url = {};
  std::string name;
  std::string value;
  time_t expires = {};
  std::string path;
  std::string domain;
  std::string comment = {};
  std::string commentURL = {};
  struct portlist *portl = {};
  char version = {};
  CookieFlags flag = {};
  Cookie *next = {};

  std::string make_cookie() const { return name + '=' + value; }

  bool match_cookie(const Url &pu, const char *domainname) const;
};

struct Str;
Str *find_cookie(const Url &pu);
int add_cookie(const Url *pu, Str *name, Str *value, time_t expires,
               Str *domain, Str *path, CookieFlags flag, Str *comment,
               int version, Str *port, Str *commentURL);
void save_cookies();
void load_cookies();
void initCookie();
bool check_cookie_accept_domain(const char *domain);

void process_http_cookie(const Url *pu, Str *lineBuf2);
std::string cookie_list_panel();
