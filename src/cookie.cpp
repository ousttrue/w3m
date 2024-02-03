/*
 * References for version 0 cookie:
 *   [NETACAPE] http://www.netscape.com/newsref/std/cookie_spec.html
 *
 * References for version 1 cookie:
 *   [RFC 2109] http://www.ics.uci.edu/pub/ietf/http/rfc2109.txt
 *   [DRAFT 12]
 * http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-man-mec-12.txt
 */
#include "cookie.h"
#include "matchattr.h"
#include "rc.h"
#include "w3m.h"
#include "terms.h"
#include "alloc.h"
#include "mytime.h"
#include "message.h"
#include "httprequest.h"
#include "readbuffer.h"
#include "textlist.h"
#include "keyvalue.h"
#include "local_cgi.h"
#include "regex.h"
#include "myctype.h"
#include "indep.h"
#include "proto.h"
#include <time.h>

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

struct portlist {
  unsigned short port;
  struct portlist *next;
};

int default_use_cookie = TRUE;
int use_cookie = TRUE;
int show_cookie = FALSE;
int accept_cookie = TRUE;
int accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;
const char *cookie_reject_domains = nullptr;
const char *cookie_accept_domains = nullptr;
const char *cookie_avoid_wrong_number_of_dots = nullptr;
TextList *Cookie_reject_domains;
TextList *Cookie_accept_domains;
TextList *Cookie_avoid_wrong_number_of_dots_domains;

static bool is_saved = 1;

#define contain_no_dots(p, ep) (total_dot_number((p), (ep), 1) == 0)

#ifdef INET6
#include <sys/socket.h>
#endif /* INET6 */
#include <netdb.h>
static const char *FQDN(const char *host) {
  const char *p;
#ifndef INET6
  struct hostent *entry;
#else  /* INET6 */
  int *af;
#endif /* INET6 */

  if (host == NULL)
    return NULL;

  if (strcasecmp(host, "localhost") == 0)
    return host;

  for (p = host; *p && *p != '.'; p++)
    ;

  if (*p == '.')
    return host;

#ifndef INET6
  if (!(entry = gethostbyname(host)))
    return NULL;

  return allocStr(entry->h_name, -1);
#else  /* INET6 */
  for (af = ai_family_order_table[DNS_order];; af++) {
    int error;
    struct addrinfo hints;
    struct addrinfo *res, *res0;
    char *namebuf;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = *af;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host, NULL, &hints, &res0);
    if (error) {
      if (*af == PF_UNSPEC) {
        /* all done */
        break;
      }
      /* try next address family */
      continue;
    }
    for (res = res0; res != NULL; res = res->ai_next) {
      if (res->ai_canonname) {
        /* found */
        namebuf = strdup(res->ai_canonname);
        freeaddrinfo(res0);
        return namebuf;
      }
    }
    freeaddrinfo(res0);
    if (*af == PF_UNSPEC) {
      break;
    }
  }
  /* all failed */
  return NULL;
#endif /* INET6 */
}

static unsigned int total_dot_number(const char *p, const char *ep,
                                     unsigned int max_count) {
  unsigned int count = 0;
  if (!ep)
    ep = p + strlen(p);

  for (; p < ep && count < max_count; p++) {
    if (*p == '.')
      count++;
  }
  return count;
}

// static int domain_match(const char *pat, const char *domain) {
//   if (domain == NULL)
//     return 0;
//   if (*pat == '.')
//     pat++;
//   for (;;) {
//     if (!strcasecmp(pat, domain))
//       return 1;
//     domain = strchr(domain, '.');
//     if (domain == NULL)
//       return 0;
//     domain++;
//   }
// }

const char *domain_match(const char *host, const char *domain) {
  int m0, m1;

  /* [RFC 2109] s. 2, "domain-match", case 1
   * (both are IP and identical)
   */
  regexCompile("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+", 0);
  m0 = regexMatch(host, -1, 1);
  m1 = regexMatch(domain, -1, 1);
  if (m0 && m1) {
    if (strcasecmp(host, domain) == 0)
      return host;
  } else if (!m0 && !m1) {
    int offset;
    const char *domain_p;
    /*
     * "." match all domains (w3m only),
     * and ".local" match local domains ([DRAFT 12] s. 2)
     */
    if (strcasecmp(domain, ".") == 0 || strcasecmp(domain, ".local") == 0) {
      offset = strlen(host);
      domain_p = &host[offset];
      if (domain[1] == '\0' || contain_no_dots(host, domain_p))
        return domain_p;
    }
    /*
     * special case for domainName = .hostName
     * see nsCookieService.cpp in Firefox.
     */
    else if (domain[0] == '.' && strcasecmp(host, &domain[1]) == 0) {
      return host;
    }
    /* [RFC 2109] s. 2, cases 2, 3 */
    else {
      offset = (domain[0] != '.') ? 0 : strlen(host) - strlen(domain);
      domain_p = &host[offset];
      if (offset >= 0 && strcasecmp(domain_p, domain) == 0)
        return domain_p;
    }
  }
  return NULL;
}

static struct portlist *make_portlist(Str *port) {
  struct portlist *first = NULL, *pl;
  char *p;
  Str *tmp = Strnew();

  p = port->ptr;
  while (*p) {
    while (*p && !IS_DIGIT(*p))
      p++;
    Strclear(tmp);
    while (*p && IS_DIGIT(*p))
      Strcat_char(tmp, *(p++));
    if (tmp->length == 0)
      break;
    pl = (struct portlist *)New(struct portlist);
    pl->port = atoi(tmp->ptr);
    pl->next = first;
    first = pl;
  }
  Strfree(tmp);
  return first;
}

static Str *portlist2str(struct portlist *first) {
  struct portlist *pl;
  Str *tmp;

  tmp = Sprintf("%d", first->port);
  for (pl = first->next; pl; pl = pl->next)
    Strcat(tmp, Sprintf(", %d", pl->port));
  return tmp;
}

static int port_match(struct portlist *first, int port) {
  struct portlist *pl;

  for (pl = first; pl; pl = pl->next) {
    if (pl->port == port)
      return 1;
  }
  return 0;
}

static void check_expired_cookies(void) {
  struct cookie *p, *p1;
  time_t now = time(NULL);

  if (!First_cookie)
    return;

  if (First_cookie->expires != (time_t)-1 && First_cookie->expires < now) {
    if (!(First_cookie->flag & COO_DISCARD))
      is_saved = 0;
    First_cookie = First_cookie->next;
  }

  for (p = First_cookie; p && p->next; p = p1) {
    p1 = p->next;
    if (p1->expires != (time_t)-1 && p1->expires < now) {
      if (!(p1->flag & COO_DISCARD))
        is_saved = 0;
      p->next = p1->next;
      p1 = p;
    }
  }
}

static Str *make_cookie(struct cookie *cookie) {
  Str *tmp = cookie->name->Strdup();
  Strcat_char(tmp, '=');
  Strcat(tmp, cookie->value);
  return tmp;
}

static int match_cookie(Url *pu, struct cookie *cookie,
                        const char *domainname) {
  if (!domainname)
    return 0;

  if (!domain_match(domainname, cookie->domain->ptr))
    return 0;
  if (strncmp(cookie->path->ptr, pu->file, cookie->path->length) != 0)
    return 0;
  if (cookie->flag & COO_SECURE && pu->schema != SCM_HTTPS)
    return 0;
  if (cookie->portl && !port_match(cookie->portl, pu->port))
    return 0;

  return 1;
}

static struct cookie *get_cookie_info(Str *domain, Str *path, Str *name) {
  struct cookie *p;

  for (p = First_cookie; p; p = p->next) {
    if (Strcasecmp(p->domain, domain) == 0 && Strcmp(p->path, path) == 0 &&
        Strcasecmp(p->name, name) == 0)
      return p;
  }
  return NULL;
}

Str *find_cookie(Url *pu) {
  Str *tmp;
  struct cookie *p, *p1, *fco = NULL;
  int version = 0;
  const char *fq_domainname, *domainname;

  fq_domainname = FQDN(pu->host);
  check_expired_cookies();
  for (p = First_cookie; p; p = p->next) {
    domainname = (p->version == 0) ? fq_domainname : pu->host;
    if (p->flag & COO_USE && match_cookie(pu, p, domainname)) {
      for (p1 = fco; p1 && Strcasecmp(p1->name, p->name); p1 = p1->next)
        ;
      if (p1)
        continue;
      p1 = (struct cookie *)New(struct cookie);
      bcopy(p, p1, sizeof(struct cookie));
      p1->next = fco;
      fco = p1;
      if (p1->version > version)
        version = p1->version;
    }
  }

  if (!fco)
    return NULL;

  tmp = Strnew();
  if (version > 0)
    Strcat(tmp, Sprintf("$Version=\"%d\"; ", version));

  Strcat(tmp, make_cookie(fco));
  for (p1 = fco->next; p1; p1 = p1->next) {
    Strcat_charp(tmp, "; ");
    Strcat(tmp, make_cookie(p1));
    if (version > 0) {
      if (p1->flag & COO_PATH)
        Strcat(tmp, Sprintf("; $Path=\"%s\"", p1->path->ptr));
      if (p1->flag & COO_DOMAIN)
        Strcat(tmp, Sprintf("; $Domain=\"%s\"", p1->domain->ptr));
      if (p1->portl)
        Strcat(tmp, Sprintf("; $Port=\"%s\"", portlist2str(p1->portl)->ptr));
    }
  }
  return tmp;
}

static int check_avoid_wrong_number_of_dots_domain(Str *domain) {
  TextListItem *tl;
  int avoid_wrong_number_of_dots_domain = FALSE;

  if (Cookie_avoid_wrong_number_of_dots_domains &&
      Cookie_avoid_wrong_number_of_dots_domains->nitem > 0) {
    for (tl = Cookie_avoid_wrong_number_of_dots_domains->first; tl != NULL;
         tl = tl->next) {
      if (domain_match(domain->ptr, tl->ptr)) {
        avoid_wrong_number_of_dots_domain = TRUE;
        break;
      }
    }
  }

  if (avoid_wrong_number_of_dots_domain == TRUE) {
    return TRUE;
  } else {
    return FALSE;
  }
}

int add_cookie(const Url *pu, Str *name, Str *value, time_t expires,
               Str *domain, Str *path, CookieFlags flag, Str *comment,
               int version, Str *port, Str *commentURL) {
  struct cookie *p;
  const char *domainname = (version == 0) ? FQDN(pu->host) : pu->host;
  Str *odomain = domain;
  Str *opath = path;
  struct portlist *portlist = NULL;
  int use_security = !(flag & COO_OVERRIDE);

#define COOKIE_ERROR(err)                                                      \
  if (!((err) & COO_OVERRIDE_OK) || use_security)                              \
  return (err)

#ifdef DEBUG
  fprintf(stderr, "host: [%s, %s] %d\n", pu->host, pu->file, flag);
  fprintf(stderr, "cookie: [%s=%s]\n", name->ptr, value->ptr);
  fprintf(stderr, "expires: [%s]\n", asctime(gmtime(&expires)));
  if (domain)
    fprintf(stderr, "domain: [%s]\n", domain->ptr);
  if (path)
    fprintf(stderr, "path: [%s]\n", path->ptr);
  fprintf(stderr, "version: [%d]\n", version);
  if (port)
    fprintf(stderr, "port: [%s]\n", port->ptr);
#endif /* DEBUG */
  /* [RFC 2109] s. 4.3.2 case 2; but this (no request-host) shouldn't happen */
  if (!domainname)
    return COO_ENODOT;

  if (domain) {
    const char *dp;
    /* [DRAFT 12] s. 4.2.2 (does not apply in the case that
     * host name is the same as domain attribute for version 0
     * cookie)
     * I think that this rule has almost the same effect as the
     * tail match of [NETSCAPE].
     */
    if (domain->ptr[0] != '.' &&
        (version > 0 || strcasecmp(domainname, domain->ptr) != 0))
      domain = Sprintf(".%s", domain->ptr);

    if (version == 0) {
      /* [NETSCAPE] rule */
      unsigned int n =
          total_dot_number(domain->ptr, domain->ptr + domain->length, 3);
      if (n < 2) {
        if (!check_avoid_wrong_number_of_dots_domain(domain)) {
          COOKIE_ERROR(COO_ESPECIAL);
        }
      }
    } else {
      /* [DRAFT 12] s. 4.3.2 case 2 */
      if (strcasecmp(domain->ptr, ".local") != 0 &&
          contain_no_dots(&domain->ptr[1], &domain->ptr[domain->length]))
        COOKIE_ERROR(COO_ENODOT);
    }

    /* [RFC 2109] s. 4.3.2 case 3 */
    if (!(dp = domain_match(domainname, domain->ptr)))
      COOKIE_ERROR(COO_EDOM);
    /* [RFC 2409] s. 4.3.2 case 4 */
    /* Invariant: dp contains matched domain */
    if (version > 0 && !contain_no_dots(domainname, dp))
      COOKIE_ERROR(COO_EBADHOST);
  }
  if (path) {
    /* [RFC 2109] s. 4.3.2 case 1 */
    if (version > 0 && strncmp(path->ptr, pu->file, path->length) != 0)
      COOKIE_ERROR(COO_EPATH);
  }
  if (port) {
    /* [DRAFT 12] s. 4.3.2 case 5 */
    portlist = make_portlist(port);
    if (portlist && !port_match(portlist, pu->port))
      COOKIE_ERROR(COO_EPORT);
  }

  if (!domain)
    domain = Strnew_charp(domainname);
  if (!path) {
    path = Strnew_charp(pu->file);
    while (path->length > 0 && Strlastchar(path) != '/')
      Strshrink(path, 1);
    if (Strlastchar(path) == '/')
      Strshrink(path, 1);
  }

  p = get_cookie_info(domain, path, name);
  if (!p) {
    p = (cookie *)New(struct cookie);
    p->flag = {};
    if (default_use_cookie) {
      p->flag = (CookieFlags)(p->flag | COO_USE);
    }
    p->next = First_cookie;
    First_cookie = p;
  }

  p->url = *pu;
  p->name = name;
  p->value = value;
  p->expires = expires;
  p->domain = domain;
  p->path = path;
  p->comment = comment;
  p->version = version;
  p->portl = portlist;
  p->commentURL = commentURL;

  if (flag & COO_SECURE)
    p->flag = (CookieFlags)(p->flag | COO_SECURE);
  else
    p->flag = (CookieFlags)(p->flag & ~COO_SECURE);

  if (odomain)
    p->flag = (CookieFlags)(p->flag | COO_DOMAIN);
  else
    p->flag = (CookieFlags)(p->flag & ~COO_DOMAIN);

  if (opath)
    p->flag = (CookieFlags)(p->flag | COO_PATH);
  else
    p->flag = (CookieFlags)(p->flag & ~COO_PATH);

  if (flag & COO_DISCARD || p->expires == (time_t)-1) {
    p->flag = (CookieFlags)(p->flag | COO_DISCARD);
  } else {
    p->flag = (CookieFlags)(p->flag & ~COO_DISCARD);
    is_saved = 0;
  }

  check_expired_cookies();
  return 0;
}

static struct cookie *nth_cookie(int n) {
  struct cookie *p;
  int i;
  for (p = First_cookie, i = 0; p; p = p->next, i++) {
    if (i == n)
      return p;
  }
  return NULL;
}

#define str2charp(str) ((str) ? (str)->ptr : "")

#define COOKIE_FILE "cookie"
void save_cookies(void) {
  struct cookie *p;
  const char *cookie_file;
  FILE *fp;

  check_expired_cookies();

  if (!First_cookie || is_saved || no_rc_dir)
    return;

  cookie_file = rcFile(COOKIE_FILE);
  if (!(fp = fopen(cookie_file, "w")))
    return;

  for (p = First_cookie; p; p = p->next) {
    if (!(p->flag & COO_USE) || p->flag & COO_DISCARD)
      continue;
    fprintf(fp, "%s\t%s\t%s\t%ld\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
            Url2Str(&p->url)->ptr, p->name->ptr, p->value->ptr,
            (long)p->expires, p->domain->ptr, p->path->ptr, p->flag, p->version,
            str2charp(p->comment),
            (p->portl) ? portlist2str(p->portl)->ptr : "",
            str2charp(p->commentURL));
  }
  fclose(fp);
  chmod(cookie_file, S_IRUSR | S_IWUSR);
}

static Str *readcol(char **p) {
  Str *tmp = Strnew();
  while (**p && **p != '\n' && **p != '\r' && **p != '\t')
    Strcat_char(tmp, *((*p)++));
  if (**p == '\t')
    (*p)++;
  return tmp;
}

void load_cookies(void) {
  struct cookie *cookie, *p;
  FILE *fp;
  Str *line;
  char *str;

  if (!(fp = fopen(rcFile(COOKIE_FILE), "r")))
    return;

  if (First_cookie) {
    for (p = First_cookie; p->next; p = p->next)
      ;
  } else {
    p = NULL;
  }
  for (;;) {
    line = Strfgets(fp);

    if (line->length == 0)
      break;
    str = line->ptr;
    cookie = (struct cookie *)New(struct cookie);
    cookie->next = NULL;
    cookie->flag = {};
    cookie->version = 0;
    cookie->expires = (time_t)-1;
    cookie->comment = NULL;
    cookie->portl = NULL;
    cookie->commentURL = NULL;
    cookie->url = Url::parse(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->name = readcol(&str);
    if (!*str)
      break;
    cookie->value = readcol(&str);
    if (!*str)
      break;
    cookie->expires = (time_t)atol(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->domain = readcol(&str);
    if (!*str)
      break;
    cookie->path = readcol(&str);
    if (!*str)
      break;
    cookie->flag = (CookieFlags)atoi(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->version = atoi(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->comment = readcol(&str);
    if (cookie->comment->length == 0)
      cookie->comment = NULL;
    if (!*str)
      break;
    cookie->portl = make_portlist(readcol(&str));
    if (!*str)
      break;
    cookie->commentURL = readcol(&str);
    if (cookie->commentURL->length == 0)
      cookie->commentURL = NULL;

    if (p)
      p->next = cookie;
    else
      First_cookie = cookie;
    p = cookie;
  }

  fclose(fp);
}

void initCookie(void) {
  load_cookies();
  check_expired_cookies();
}

Buffer *cookie_list_panel(void) {
  /* FIXME: gettextize? */
  Str *src = Strnew_charp("<html><head><title>Cookies</title></head>"
                          "<body><center><b>Cookies</b></center>"
                          "<p><form method=internal action=cookie>");
  struct cookie *p;
  int i;
  const char *tmp;
  char tmp2[80];

  if (!use_cookie || !First_cookie)
    return NULL;

  Strcat_charp(src, "<ol>");
  for (p = First_cookie, i = 0; p; p = p->next, i++) {
    tmp = html_quote(Url2Str(&p->url)->ptr);
    if (p->expires != (time_t)-1) {
#ifdef HAVE_STRFTIME
      strftime(tmp2, 80, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&p->expires));
#else  /* not HAVE_STRFTIME */
      struct tm *gmt;
      static const char *dow[] = {"Sun ", "Mon ", "Tue ", "Wed ",
                                  "Thu ", "Fri ", "Sat "};
      static const char *month[] = {"Jan ", "Feb ", "Mar ", "Apr ",
                                    "May ", "Jun ", "Jul ", "Aug ",
                                    "Sep ", "Oct ", "Nov ", "Dec "};
      gmt = gmtime(&p->expires);
      strcpy(tmp2, dow[gmt->tm_wday]);
      sprintf(&tmp2[4], "%02d ", gmt->tm_mday);
      strcpy(&tmp2[7], month[gmt->tm_mon]);
      if (gmt->tm_year < 1900)
        sprintf(&tmp2[11], "%04d %02d:%02d:%02d GMT", (gmt->tm_year) + 1900,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
      else
        sprintf(&tmp2[11], "%04d %02d:%02d:%02d GMT", gmt->tm_year,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
#endif /* not HAVE_STRFTIME */
    } else
      tmp2[0] = '\0';
    Strcat_charp(src, "<li>");
    Strcat_charp(src, "<h1><a href=\"");
    Strcat_charp(src, tmp);
    Strcat_charp(src, "\">");
    Strcat_charp(src, tmp);
    Strcat_charp(src, "</a></h1>");

    Strcat_charp(src, "<table cellpadding=0>");
    if (!(p->flag & COO_SECURE)) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Cookie:</b></td><td>");
      Strcat_charp(src, html_quote(make_cookie(p)->ptr));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->comment) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Comment:</b></td><td>");
      Strcat_charp(src, html_quote(p->comment->ptr));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->commentURL) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>CommentURL:</b></td><td>");
      Strcat_charp(src, "<a href=\"");
      Strcat_charp(src, html_quote(p->commentURL->ptr));
      Strcat_charp(src, "\">");
      Strcat_charp(src, html_quote(p->commentURL->ptr));
      Strcat_charp(src, "</a>");
      Strcat_charp(src, "</td></tr>");
    }
    if (tmp2[0]) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Expires:</b></td><td>");
      Strcat_charp(src, tmp2);
      if (p->flag & COO_DISCARD)
        Strcat_charp(src, " (Discard)");
      Strcat_charp(src, "</td></tr>");
    }
    Strcat_charp(src, "<tr><td width=\"80\"><b>Version:</b></td><td>");
    Strcat_charp(src, Sprintf("%d", p->version)->ptr);
    Strcat_charp(src, "</td></tr><tr><td>");
    if (p->domain) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Domain:</b></td><td>");
      Strcat_charp(src, html_quote(p->domain->ptr));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->path) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Path:</b></td><td>");
      Strcat_charp(src, html_quote(p->path->ptr));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->portl) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Port:</b></td><td>");
      Strcat_charp(src, html_quote(portlist2str(p->portl)->ptr));
      Strcat_charp(src, "</td></tr>");
    }
    Strcat_charp(src, "<tr><td width=\"80\"><b>Secure:</b></td><td>");
    Strcat_charp(src, (p->flag & COO_SECURE) ? "Yes" : "No");
    Strcat_charp(src, "</td></tr><tr><td>");

    Strcat(src, Sprintf("<tr><td width=\"80\"><b>Use:</b></td><td>"
                        "<input type=radio name=\"%d\" value=1%s>Yes"
                        "&nbsp;&nbsp;"
                        "<input type=radio name=\"%d\" value=0%s>No",
                        i, (p->flag & COO_USE) ? " checked" : "", i,
                        (!(p->flag & COO_USE)) ? " checked" : ""));
    Strcat_charp(
        src, "</td></tr><tr><td><input type=submit value=\"OK\"></table><p>");
  }
  Strcat_charp(src, "</ol></form></body></html>");
  return loadHTMLString(src);
}

void set_cookie_flag(struct keyvalue *arg) {
  int n, v;
  struct cookie *p;

  while (arg) {
    if (arg->arg && *arg->arg && arg->value && *arg->value) {
      n = atoi(arg->arg);
      v = atoi(arg->value);
      if ((p = nth_cookie(n)) != NULL) {
        if (v && !(p->flag & COO_USE))
          p->flag = (CookieFlags)(p->flag | COO_USE);
        else if (!v && p->flag & COO_USE)
          p->flag = (CookieFlags)(p->flag & ~COO_USE);
        if (!(p->flag & COO_DISCARD))
          is_saved = 0;
      }
    }
    arg = arg->next;
  }
  backBf();
}

int check_cookie_accept_domain(const char *domain) {
  TextListItem *tl;

  if (domain == NULL)
    return 0;

  if (Cookie_accept_domains && Cookie_accept_domains->nitem > 0) {
    for (tl = Cookie_accept_domains->first; tl != NULL; tl = tl->next) {
      if (domain_match(domain, tl->ptr))
        return 1;
    }
  }
  if (Cookie_reject_domains && Cookie_reject_domains->nitem > 0) {
    for (tl = Cookie_reject_domains->first; tl != NULL; tl = tl->next) {
      if (domain_match(domain, tl->ptr))
        return 0;
    }
  }
  return 1;
}

/* This array should be somewhere else */
const char *violations[COO_EMAX] = {
    "internal error",          "tail match failed",
    "wrong number of dots",    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1", "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",   "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

void process_http_cookie(const Url *pu, Str *lineBuf2) {
  Str *name = Strnew();
  Str *value = Strnew();
  Str *domain = NULL;
  Str *path = NULL;
  Str *comment = NULL;
  Str *commentURL = NULL;
  Str *port = NULL;
  Str *tmp2;
  int version;
  int quoted;
  CookieFlags flag = {};
  time_t expires = (time_t)-1;

  const char *q = {};
  const char *p = {};
  if (lineBuf2->ptr[10] == '2') {
    p = lineBuf2->ptr + 12;
    version = 1;
  } else {
    p = lineBuf2->ptr + 11;
    version = 0;
  }

#ifdef DEBUG
  fprintf(stderr, "Set-Cookie: [%s]\n", p);
#endif /* DEBUG */

  SKIP_BLANKS(p);
  while (*p != '=' && !IS_ENDT(*p))
    Strcat_char(name, *(p++));
  Strremovetrailingspaces(name);
  if (*p == '=') {
    p++;
    SKIP_BLANKS(p);
    quoted = 0;
    while (!IS_ENDL(*p) && (quoted || *p != ';')) {
      if (!IS_SPACE(*p))
        q = p;
      if (*p == '"')
        quoted = (quoted) ? 0 : 1;
      Strcat_char(value, *(p++));
    }
    if (q)
      Strshrink(value, p - q - 1);
  }
  while (*p == ';') {
    p++;
    SKIP_BLANKS(p);
    if (matchattr(p, "expires", 7, &tmp2)) {
      /* version 0 */
      expires = mymktime(tmp2->ptr);
    } else if (matchattr(p, "max-age", 7, &tmp2)) {
      /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2
       */
      expires = time(NULL) + atol(tmp2->ptr);
    } else if (matchattr(p, "domain", 6, &tmp2)) {
      domain = tmp2;
    } else if (matchattr(p, "path", 4, &tmp2)) {
      path = tmp2;
    } else if (matchattr(p, "secure", 6, NULL)) {
      flag = (CookieFlags)(flag | COO_SECURE);
    } else if (matchattr(p, "comment", 7, &tmp2)) {
      comment = tmp2;
    } else if (matchattr(p, "version", 7, &tmp2)) {
      version = atoi(tmp2->ptr);
    } else if (matchattr(p, "port", 4, &tmp2)) {
      /* version 1, Set-Cookie2 */
      port = tmp2;
    } else if (matchattr(p, "commentURL", 10, &tmp2)) {
      /* version 1, Set-Cookie2 */
      commentURL = tmp2;
    } else if (matchattr(p, "discard", 7, NULL)) {
      /* version 1, Set-Cookie2 */
      flag = (CookieFlags)(flag | COO_DISCARD);
    }
    quoted = 0;
    while (!IS_ENDL(*p) && (quoted || *p != ';')) {
      if (*p == '"')
        quoted = (quoted) ? 0 : 1;
      p++;
    }
  }
  if (pu && name->length > 0) {
    int err;
    if (show_cookie) {
      if (flag & COO_SECURE)
        disp_message_nsec("Received a secured cookie", FALSE, 1, TRUE, FALSE);
      else
        disp_message_nsec(
            Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr,
            FALSE, 1, TRUE, FALSE);
    }
    err = add_cookie(pu, name, value, expires, domain, path, flag, comment,
                     version, port, commentURL);
    if (err) {
      const char *ans =
          (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT) ? "y" : NULL;
      if (fmInitialized && (err & COO_OVERRIDE_OK) &&
          accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
        Str *msg =
            Sprintf("Accept bad cookie from %s for %s?", pu->host,
                    ((domain && domain->ptr) ? domain->ptr : "<localdomain>"));
        if (msg->length > COLS - 10)
          Strshrink(msg, msg->length - (COLS - 10));
        Strcat_charp(msg, " (y/n)");
        // ans = inputAnswer(msg->ptr);
        ans = "y";
      }

      if (ans == NULL || TOLOWER(*ans) != 'y' ||
          (err = add_cookie(pu, name, value, expires, domain, path,
                            (CookieFlags)(flag | COO_OVERRIDE), comment,
                            version, port, commentURL))) {
        err = (err & ~COO_OVERRIDE_OK) - 1;
        const char *emsg;
        if (err >= 0 && err < COO_EMAX)
          emsg = Sprintf("This cookie was rejected "
                         "to prevent security violation. [%s]",
                         violations[err])
                     ->ptr;
        else
          emsg = "This cookie was rejected to prevent security violation.";
        record_err_message(emsg);
        if (show_cookie)
          disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
      } else if (show_cookie)
        disp_message_nsec(
            Sprintf("Accepting invalid cookie: %s=%s", name->ptr, value->ptr)
                ->ptr,
            FALSE, 1, TRUE, FALSE);
    }
  }
}
