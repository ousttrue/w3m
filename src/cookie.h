#pragma once
#include "url.h"
#include <time.h>

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
struct TextList;
extern TextList *Cookie_reject_domains;
extern TextList *Cookie_accept_domains;
extern TextList *Cookie_avoid_wrong_number_of_dots_domains;

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
struct cookie {
  Url url;
  Str *name;
  Str *value;
  time_t expires;
  Str *path;
  Str *domain;
  Str *comment;
  Str *commentURL;
  struct portlist *portl;
  char version;
  CookieFlags flag;
  struct cookie *next;
};

const char *domain_match(const char *host, const char *domain);
Str *find_cookie(Url *pu);
int add_cookie(const Url *pu, Str *name, Str *value, time_t expires,
               Str *domain, Str *path, CookieFlags flag, Str *comment,
               int version, Str *port, Str *commentURL);
void save_cookies();
void load_cookies();
void initCookie();
void cooLst();
int check_cookie_accept_domain(const char *domain);

void process_http_cookie(const Url *pu, Str *lineBuf2);
struct Buffer;
Buffer *cookie_list_panel();
