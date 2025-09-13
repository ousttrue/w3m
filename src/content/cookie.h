#pragma once
#include "geometry.h"
#include "url.h"
#include "textlist.h"
#include <Str.h>
#include <time.h>

extern bool use_cookie;
extern int default_use_cookie;
extern char* cookie_reject_domains;
extern char* cookie_accept_domains;
extern char* cookie_avoid_wrong_number_of_dots;
extern TextList* Cookie_reject_domains;
extern TextList* Cookie_accept_domains;
extern TextList* Cookie_avoid_wrong_number_of_dots_domains;
extern int no_rc_dir;

extern int accept_cookie;
extern int show_cookie;
enum AcceptBadCookieMode {
    ACCEPT_BAD_COOKIE_DISCARD = 0,
    ACCEPT_BAD_COOKIE_ACCEPT = 1,
    ACCEPT_BAD_COOKIE_ASK = 2,
};
extern enum AcceptBadCookieMode accept_bad_cookie;

enum CookieViolation {
    COO_OVERRIDE_OK = 32 /* flag to specify that an error is overridable */,
    /* version 0 refers to the original cookie_spec.html */
    /* version 1 refers to RFC 2109 */
    /* version 1' refers to the Internet draft to obsolete RFC 2109 */
    COO_EINTERNAL = (1) /* unknown error; probably forgot to convert "return 1" in cookie.c */,
    COO_ETAIL = (2 | COO_OVERRIDE_OK) /* tail match failed (version 0) */,
    COO_ESPECIAL = (3) /* special domain check failed (version 0) */,
    COO_EPATH = (4) /* Path attribute mismatch (version 1 case 1) */,
    COO_ENODOT = (5 | COO_OVERRIDE_OK) /* no embedded dots in Domain (version 1 case 2.1) */,
    COO_ENOTV1DOM = (6 | COO_OVERRIDE_OK) /* Domain does not start with a dot (version 1 case 2.2) */,
    COO_EDOM = (7 | COO_OVERRIDE_OK) /* domain-match failed (version 1 case 3) */,
    COO_EBADHOST = (8 | COO_OVERRIDE_OK) /* dot in matched host name in FQDN (version 1 case 4) */,
    COO_EPORT = (9) /* Port match failed (version 1' case 5) */,
    COO_EMAX = COO_EPORT,
};
inline static const char* getCookieViolationMsg(int err)
{
    // This array should be somewhere else
    static const char* violations[COO_EMAX] = {
        "internal error",
        "tail match failed",
        "wrong number of dots",
        "RFC 2109 4.3.2 rule 1",
        "RFC 2109 4.3.2 rule 2.1",
        "RFC 2109 4.3.2 rule 2.2",
        "RFC 2109 4.3.2 rule 3",
        "RFC 2109 4.3.2 rule 4",
        "RFC XXXX 4.3.2 rule 5"
    };
    return violations[err];
}

struct portlist {
    unsigned short port;
    struct portlist* next;
};

enum CookieFlag {
    COO_USE = 1,
    COO_SECURE = 2,
    COO_DOMAIN = 4,
    COO_PATH = 8,
    COO_DISCARD = 16,
    COO_OVERRIDE = 32 /* user chose to override security checks */,
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
    struct portlist* portl;
    char version;
    enum CookieFlag flag;
    struct cookie* next;
};

// global struct auth_cookie* Auth_cookie init(NULL);
extern struct cookie* First_cookie;
void save_cookies();
void load_cookies();
void initCookie();

Str find_cookie(struct Url* pu);
int add_cookie(struct Url* pu, Str name, Str value, time_t expires,
    Str domain, Str path, int flag, Str comment, int version,
    Str port, Str commentURL);
struct KeyValue;
void set_cookie_flag(struct UI ui, struct KeyValue* arg);
bool check_cookie_accept_domain(const char* domain);
Str make_cookie(struct cookie* cookie);
Str portlist2str(struct portlist* first);
