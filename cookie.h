#ifndef W3M_COOKIE_H
#define W3M_COOKIE_H

#include "Str.h"
#include "buffer.h"
#include "html.h"
#include "parsetag.h"

#include <time.h>

#define COO_USE		1
#define COO_SECURE	2
#define COO_DOMAIN	4
#define COO_PATH	8
#define COO_DISCARD	16
/* user chose to override security checks */
#define COO_OVERRIDE	32
/* flag to specify that an error is overridable
   version 0 refers to the original cookie_spec.html
   version 1 refers to RFC 2109
   version 1' refers to the Internet draft to obsolete RFC 2109
   */
#define COO_OVERRIDE_OK	32
/* unknown error; probably forgot to convert "return 1" in cookie.c */
#define COO_EINTERNAL	(1)
/* tail match failed (version 0) */
#define COO_ETAIL	(2 | COO_OVERRIDE_OK)
/* special domain check failed (version 0) */
#define COO_ESPECIAL	(3)
/* Path attribute mismatch (version 1 case 1) */
#define COO_EPATH	(4)
/* no embedded dots in Domain (version 1 case 2.1) */
#define COO_ENODOT	(5 | COO_OVERRIDE_OK)
/* Domain does not start with a dot (version 1 case 2.2) */
#define COO_ENOTV1DOM	(6 | COO_OVERRIDE_OK)
/* domain-match failed (version 1 case 3) */
#define COO_EDOM	(7 | COO_OVERRIDE_OK)
/* dot in matched host name in FQDN (version 1 case 4) */
#define COO_EBADHOST	(8 | COO_OVERRIDE_OK)
/* Port match failed (version 1' case 5) */
#define COO_EPORT	(9)
#define COO_EMAX	COO_EPORT

#define ACCEPT_BAD_COOKIE_DISCARD	0
#define ACCEPT_BAD_COOKIE_ACCEPT	1
#define ACCEPT_BAD_COOKIE_ASK		2

Buffer *cookie_list_panel(void);
Str find_cookie(ParsedURL *pu);
int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
	       Str domain, Str path, int flag, Str comment, int version,
	       Str port, Str commentURL);
int check_cookie_accept_domain(const char *domain);
void cooLst(void);
void initCookie(void);
void parse_cookie(void);
void save_cookies(void);
void set_cookie_flag(struct parsed_tagarg *arg);

extern char *CookieFile;

#endif
