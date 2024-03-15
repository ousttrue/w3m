#pragma once
#include "url.h"
#include "Str.h"
#include <time.h>
#include <gc_cpp.h>
#include <string>
#include <memory>
#include <list>

extern int default_use_cookie;
extern int use_cookie;
extern int show_cookie;
extern int accept_cookie;
#define ACCEPT_BAD_COOKIE_DISCARD 0
#define ACCEPT_BAD_COOKIE_ACCEPT 1
#define ACCEPT_BAD_COOKIE_ASK 2
extern int accept_bad_cookie;
extern const char *cookie_reject_domains;
extern const char *cookie_accept_domains;
extern const char *cookie_avoid_wrong_number_of_dots;
extern std::list<std::string> Cookie_reject_domains;
extern std::list<std::string> Cookie_accept_domains;
extern std::list<std::string> Cookie_avoid_wrong_number_of_dots_domains;

struct Str;
enum CookieFlags {
  COO_NONE = 0,
  COO_USE = 1,
  COO_SECURE = 2,
  COO_DOMAIN = 4,
  COO_PATH = 8,
  COO_DISCARD = 16,
  COO_OVERRIDE = 32, /* user chose to override security checks */
};

const char *domain_match(const char *host, const char *domain);
int port_match(struct portlist *first, int port);

struct Cookie : public gc_cleanup {
  Url url = {};
  std::string name;
  std::string value;
  time_t expires = {};
  std::string path;
  std::string domain;
  Str *comment = {};
  Str *commentURL = {};
  struct portlist *portl = {};
  char version = {};
  CookieFlags flag = {};
  Cookie *next = {};

  std::string make_cookie() const { return name + '=' + value; }

  bool match_cookie(const Url &pu, const char *domainname) const {
    if (!domainname) {
      return 0;
    }
    if (!domain_match(domainname, this->domain.c_str()))
      return 0;
    if (!pu.file.starts_with(this->path))
      return 0;
    if (this->flag & COO_SECURE && pu.scheme != SCM_HTTPS)
      return 0;
    if (this->portl && !port_match(this->portl, pu.port))
      return 0;

    return 1;
  }
};

Str *find_cookie(const Url &pu);
int add_cookie(const Url *pu, Str *name, Str *value, time_t expires,
               Str *domain, Str *path, CookieFlags flag, Str *comment,
               int version, Str *port, Str *commentURL);
void save_cookies();
void load_cookies();
void initCookie();
bool check_cookie_accept_domain(const char *domain);

void process_http_cookie(const Url *pu, Str *lineBuf2);
struct Buffer;
std::shared_ptr<Buffer> cookie_list_panel();
