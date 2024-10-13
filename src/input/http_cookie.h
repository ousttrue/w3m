#pragma once
#include "input/url.h"
#include "text/Str.h"
#include <time.h>

#define ACCEPT_BAD_COOKIE_DISCARD 0
#define ACCEPT_BAD_COOKIE_ACCEPT 1
#define ACCEPT_BAD_COOKIE_ASK 2

extern int default_use_cookie;
extern int use_cookie;
extern int show_cookie;
extern int accept_cookie;
extern int accept_bad_cookie;
extern char *cookie_reject_domains;
extern char *cookie_accept_domains;
extern char *cookie_avoid_wrong_number_of_dots;
extern struct TextList *Cookie_reject_domains;
extern struct TextList *Cookie_accept_domains;
extern struct TextList *Cookie_avoid_wrong_number_of_dots_domains;

struct portlist {
  unsigned short port;
  struct portlist *next;
};

struct cookie {
  struct Url url;
  Str name;
  Str value;
  time_t expires;
  Str path;
  Str domain;
  Str comment;
  Str commentURL;
  struct portlist *portl;
  char version;
  char flag;
  struct cookie *next;
};
#define COO_USE 1
#define COO_SECURE 2
#define COO_DOMAIN 4
#define COO_PATH 8
#define COO_DISCARD 16
#define COO_OVERRIDE 32 /* user chose to override security checks */

#define COO_OVERRIDE_OK 32 /* flag to specify that an error is overridable */
/* version 0 refers to the original cookie_spec.html */
/* version 1 refers to RFC 2109 */
/* version 1' refers to the Internet draft to obsolete RFC 2109 */
#define COO_EINTERNAL                                                          \
  (1) /* unknown error; probably forgot to convert "return 1" in cookie.c */
#define COO_ETAIL (2 | COO_OVERRIDE_OK) /* tail match failed (version 0) */
#define COO_ESPECIAL (3) /* special domain check failed (version 0) */
#define COO_EPATH (4)    /* Path attribute mismatch (version 1 case 1) */
#define COO_ENODOT                                                             \
  (5 | COO_OVERRIDE_OK) /* no embedded dots in Domain (version 1 case 2.1) */
#define COO_ENOTV1DOM                                                          \
  (6 | COO_OVERRIDE_OK) /* Domain does not start with a dot (version 1         \
                           case 2.2) */
#define COO_EDOM                                                               \
  (7 | COO_OVERRIDE_OK) /* domain-match failed (version 1 case 3) */
#define COO_EBADHOST                                                           \
  (8 |                                                                         \
   COO_OVERRIDE_OK)   /* dot in matched host name in FQDN (version 1 case 4) */
#define COO_EPORT (9) /* Port match failed (version 1' case 5) */
#define COO_EMAX COO_EPORT

Str find_cookie(struct Url *pu);
int add_cookie(struct Url *pu, Str name, Str value, time_t expires, Str domain,
               Str path, int flag, Str comment, int version, Str port,
               Str commentURL);
void save_cookies(void);
void load_cookies(void);
void initCookie(void);
struct Document *cookie_list_panel(void);
struct HtmlTag;
void set_cookie_flag(struct HtmlTag *arg);
int check_cookie_accept_domain(char *domain);
