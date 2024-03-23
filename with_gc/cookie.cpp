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
#include "quote.h"
#include "html/html_quote.h"
#include "matchattr.h"
#include "alloc.h"
#include "keyvalue.h"
#include "regex.h"
#include "myctype.h"
#include "Str.h"
#include "dns_order.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gc_cpp.h>
#include <time.h>
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#endif

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

struct Cookie *First_cookie = nullptr;

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
   COO_OVERRIDE_OK) /* dot in matched host name in FQDN (version 1 case 4) */

struct portlist {
  unsigned short port;
  struct portlist *next;
};

bool no_rc_dir = false;
int DNS_order = DNS_ORDER_UNSPEC;
bool default_use_cookie = true;
bool use_cookie = true;
bool show_cookie = false;
bool accept_cookie = true;
int accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;
std::string cookie_reject_domains;
std::string cookie_accept_domains;
std::string cookie_avoid_wrong_number_of_dots;
std::list<std::string> Cookie_reject_domains;
std::list<std::string> Cookie_accept_domains;
std::list<std::string> Cookie_avoid_wrong_number_of_dots_domains;

static bool is_saved = 1;

#ifdef INET6
#ifdef _MSC_VER
#else
#include <sys/socket.h>
#endif
#endif /* INET6 */

#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <netdb.h>
#endif
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

/// if match, return host.substr
static std::optional<std::string_view> domain_match(std::string_view host,
                                                    std::string_view _domain) {
  // std::string host(_host.begin(), _host.end());
  std::string domain(_domain.begin(), _domain.end());

  /* [RFC 2109] s. 2, "domain-match", case 1
   * (both are IP and identical)
   */
  regexCompile("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+", 0);
  auto m0 = regexMatch(host.data(), host.size(), 1);
  auto m1 = regexMatch(domain.c_str(), -1, 1);
  if (m0 && m1) {
    if (strcasecmp(host, domain.c_str()) == 0)
      return host;
  } else if (!m0 && !m1) {
    int offset;
    /*
     * "." match all domains (w3m only),
     * and ".local" match local domains ([DRAFT 12] s. 2)
     */
    if (strcasecmp(domain.c_str(), ".") == 0 ||
        strcasecmp(domain.c_str(), ".local") == 0) {
      offset = host.size();
      auto domain_p = &host[offset];
      if (domain[1] == '\0' || contain_no_dots({host.data(), domain_p}))
        return host.substr(offset);
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
      offset = (domain[0] != '.') ? 0 : host.size() - domain.size();
      auto domain_p = &host[offset];
      if (offset >= 0 && strcasecmp(domain_p, domain.c_str()) == 0)
        return host.substr(offset);
    }
  }
  return {};
}

static struct portlist *make_portlist(std::string_view port) {
  struct portlist *first = NULL;
  Str *tmp = Strnew();
  auto p = port.begin();
  while (p != port.end()) {
    while (*p && !IS_DIGIT(*p))
      p++;
    Strclear(tmp);
    while (*p && IS_DIGIT(*p))
      Strcat_char(tmp, *(p++));
    if (tmp->length == 0)
      break;
    auto pl = (struct portlist *)New(struct portlist);
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

int port_match(struct portlist *first, int port) {
  struct portlist *pl;

  for (pl = first; pl; pl = pl->next) {
    if (pl->port == port)
      return 1;
  }
  return 0;
}

static void check_expired_cookies(void) {
  struct Cookie *p, *p1;
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

static struct Cookie *get_cookie_info(std::string_view domain,
                                      std::string_view path,
                                      std::string_view name) {
  for (auto p = First_cookie; p; p = p->next) {
    if (p->domain == domain && p->path == path && p->name == name)
      return p;
  }
  return {};
}

std::optional<std::string> find_cookie(const Url &pu) {
  Str *tmp;
  struct Cookie *p, *p1, *fco = NULL;
  int version = 0;
  const char *fq_domainname, *domainname;

  fq_domainname = FQDN(pu.host.c_str());
  check_expired_cookies();
  for (p = First_cookie; p; p = p->next) {
    domainname = (p->version == 0) ? fq_domainname : pu.host.c_str();
    if (p->flag & COO_USE && p->match_cookie(pu, domainname)) {
      for (p1 = fco; p1 && p1->name != p->name; p1 = p1->next)
        ;
      if (p1)
        continue;
      p1 = new Cookie;
      *p1 = *p;
      p1->next = fco;
      fco = p1;
      if (p1->version > version)
        version = p1->version;
    }
  }

  if (!fco)
    return {};

  tmp = Strnew();
  if (version > 0)
    Strcat(tmp, Sprintf("$Version=\"%d\"; ", version));

  Strcat(tmp, fco->make_cookie());
  for (p1 = fco->next; p1; p1 = p1->next) {
    Strcat_charp(tmp, "; ");
    Strcat(tmp, p1->make_cookie());
    if (version > 0) {
      if (p1->flag & COO_PATH)
        Strcat(tmp, Sprintf("; $Path=\"%s\"", p1->path.c_str()));
      if (p1->flag & COO_DOMAIN)
        Strcat(tmp, Sprintf("; $Domain=\"%s\"", p1->domain.c_str()));
      if (p1->portl)
        Strcat(tmp, Sprintf("; $Port=\"%s\"", portlist2str(p1->portl)->ptr));
    }
  }
  return std::string(tmp->ptr);
}

static bool check_avoid_wrong_number_of_dots_domain(std::string_view domain) {
  bool avoid_wrong_number_of_dots_domain = false;
  for (auto &tl : Cookie_avoid_wrong_number_of_dots_domains) {
    if (domain_match(domain, tl)) {
      avoid_wrong_number_of_dots_domain = true;
      break;
    }
  }

  if (avoid_wrong_number_of_dots_domain == true) {
    return true;
  } else {
    return false;
  }
}

int add_cookie(const Url *pu, std::string_view name, std::string_view value,
               time_t expires, std::string_view domain, std::string_view _path,
               CookieFlags flag, std::string_view comment, int version,
               std::string_view port, std::string_view commentURL) {
  struct Cookie *p;
  std::string domainname = (version == 0) ? FQDN(pu->host.c_str()) : pu->host;
  std::string odomain(domain.begin(), domain.end());
  std::string path(_path.begin(), _path.end());
  std::string opath = path;
  struct portlist *portlist = NULL;
  int use_security = !(flag & COO_OVERRIDE);

#define COOKIE_ERROR(err)                                                      \
  if (!((err) & COO_OVERRIDE_OK) || use_security)                              \
  return (err)

  /* [RFC 2109] s. 4.3.2 case 2; but this (no request-host) shouldn't happen */
  if (domainname.empty()) {
    return COO_ENODOT;
  }

  if (domain.size()) {
    /* [DRAFT 12] s. 4.2.2 (does not apply in the case that
     * host name is the same as domain attribute for version 0
     * cookie)
     * I think that this rule has almost the same effect as the
     * tail match of [NETSCAPE].
     */
    if (domain[0] != '.' && (version > 0 || domainname != domain))
      domain = std::string(".") + std::string(domain.begin(), domain.end());

    if (version == 0) {
      /* [NETSCAPE] rule */
      unsigned int n = total_dot_number(domain, 3);
      if (n < 2) {
        if (!check_avoid_wrong_number_of_dots_domain(domain)) {
          COOKIE_ERROR(COO_ESPECIAL);
        }
      }
    } else {
      /* [DRAFT 12] s. 4.3.2 case 2 */
      if (strcasecmp(domain, ".local") != 0 &&
          contain_no_dots(domain.substr(1)))
        COOKIE_ERROR(COO_ENODOT);
    }

    /* [RFC 2109] s. 4.3.2 case 3 */
    std::optional<std::string> dp;
    if (!(dp = domain_match(domainname, domain)))
      COOKIE_ERROR(COO_EDOM);
    /* [RFC 2409] s. 4.3.2 case 4 */
    /* Invariant: dp contains matched domain */
    if (version > 0 && !contain_no_dots({domainname.c_str(), dp->data()}))
      COOKIE_ERROR(COO_EBADHOST);
  }
  if (path.size()) {
    /* [RFC 2109] s. 4.3.2 case 1 */
    if (version > 0 && !pu->file.starts_with(path))
      COOKIE_ERROR(COO_EPATH);
  }
  if (port.size()) {
    /* [DRAFT 12] s. 4.3.2 case 5 */
    portlist = make_portlist(port);
    if (portlist && !port_match(portlist, pu->port))
      COOKIE_ERROR(COO_EPORT);
  }

  if (domain.empty()) {
    domain = domainname;
  }
  if (path.empty()) {
    path = pu->file;
    while (path.size() && path.back() != '/') {
      path.pop_back();
    }
  }

  p = get_cookie_info(domain, path, name);
  if (!p) {
    p = new Cookie;
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

  if (odomain.size())
    p->flag = (CookieFlags)(p->flag | COO_DOMAIN);
  else
    p->flag = (CookieFlags)(p->flag & ~COO_DOMAIN);

  if (opath.size())
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

static struct Cookie *nth_cookie(int n) {
  struct Cookie *p;
  int i;
  for (p = First_cookie, i = 0; p; p = p->next, i++) {
    if (i == n)
      return p;
  }
  return NULL;
}

void save_cookies(const std::string &cookie_file) {

  check_expired_cookies();

  if (!First_cookie || is_saved || no_rc_dir)
    return;

  FILE *fp;
  if (!(fp = fopen(cookie_file.c_str(), "w")))
    return;

  struct Cookie *p;
  for (p = First_cookie; p; p = p->next) {
    if (!(p->flag & COO_USE) || p->flag & COO_DISCARD)
      continue;
    fprintf(fp, "%s\t%s\t%s\t%ld\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
            p->url.to_Str().c_str(), p->name.c_str(), p->value.c_str(),
            (long)p->expires, p->domain.c_str(), p->path.c_str(), p->flag,
            p->version, p->comment.c_str(),
            (p->portl) ? portlist2str(p->portl)->ptr : "",
            p->commentURL.c_str());
  }
  fclose(fp);

#ifdef _MSC_VER
#else
  chmod(cookie_file, S_IRUSR | S_IWUSR);
#endif
}

static Str *readcol(char **p) {
  Str *tmp = Strnew();
  while (**p && **p != '\n' && **p != '\r' && **p != '\t')
    Strcat_char(tmp, *((*p)++));
  if (**p == '\t')
    (*p)++;
  return tmp;
}

void load_cookies(const std::string &cookie_file) {
  FILE *fp;
  if (!(fp = fopen(cookie_file.c_str(), "r")))
    return;

  Cookie *p;
  if (First_cookie) {
    for (p = First_cookie; p->next; p = p->next)
      ;
  } else {
    p = NULL;
  }

  Str *line;
  char *str;
  for (;;) {
    line = Strfgets(fp);

    if (line->length == 0)
      break;
    str = line->ptr;
    auto cookie = new struct Cookie;
    cookie->next = NULL;
    cookie->flag = {};
    cookie->version = 0;
    cookie->expires = (time_t)-1;
    cookie->comment = {};
    cookie->portl = NULL;
    cookie->commentURL = {};
    cookie->url = {readcol(&str)->ptr};
    if (!*str)
      break;
    cookie->name = readcol(&str)->ptr;
    if (!*str)
      break;
    cookie->value = readcol(&str)->ptr;
    if (!*str)
      break;
    cookie->expires = (time_t)atol(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->domain = readcol(&str)->ptr;
    if (!*str)
      break;
    cookie->path = readcol(&str)->ptr;
    if (!*str)
      break;
    cookie->flag = (CookieFlags)atoi(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->version = atoi(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->comment = readcol(&str)->ptr;
    if (!*str)
      break;
    cookie->portl = make_portlist(readcol(&str)->ptr);
    if (!*str)
      break;
    cookie->commentURL = readcol(&str)->ptr;

    if (p)
      p->next = cookie;
    else
      First_cookie = cookie;
    p = cookie;
  }

  fclose(fp);
}

void initCookie(const std::string &cookie_file) {
  load_cookies(cookie_file);
  check_expired_cookies();
}

std::string cookie_list_panel() {
  Str *src = Strnew_charp("<html><head><title>Cookies</title></head>"
                          "<body><center><b>Cookies</b></center>"
                          "<p><form method=internal action=cookie>");
  struct Cookie *p;
  int i;
  const char *tmp;
  char tmp2[80];

  if (!use_cookie || !First_cookie)
    return NULL;

  Strcat_charp(src, "<ol>");
  for (p = First_cookie, i = 0; p; p = p->next, i++) {
    tmp = html_quote(p->url.to_Str().c_str());
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
      snprintf(&tmp2[4], sizeof(tmp2) - 4, "%02d ", gmt->tm_mday);
      strcpy(&tmp2[7], month[gmt->tm_mon]);
      if (gmt->tm_year < 1900) {
        snprintf(&tmp2[11], sizeof(tmp2) - 11, "%04d %02d:%02d:%02d GMT",
                 (gmt->tm_year) + 1900, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
      } else {
        snprintf(&tmp2[11], sizeof(tmp2) - 11, "%04d %02d:%02d:%02d GMT",
                 gmt->tm_year, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
      }
#endif /* not HAVE_STRFTIME */
    } else {
      tmp2[0] = '\0';
    }
    Strcat_charp(src, "<li>");
    Strcat_charp(src, "<h1><a href=\"");
    Strcat_charp(src, tmp);
    Strcat_charp(src, "\">");
    Strcat_charp(src, tmp);
    Strcat_charp(src, "</a></h1>");

    Strcat_charp(src, "<table cellpadding=0>");
    if (!(p->flag & COO_SECURE)) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Cookie:</b></td><td>");
      Strcat_charp(src, html_quote(p->make_cookie().c_str()));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->comment.size()) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Comment:</b></td><td>");
      Strcat_charp(src, html_quote(p->comment.c_str()));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->commentURL.size()) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>CommentURL:</b></td><td>");
      Strcat_charp(src, "<a href=\"");
      Strcat_charp(src, html_quote(p->commentURL.c_str()));
      Strcat_charp(src, "\">");
      Strcat_charp(src, html_quote(p->commentURL.c_str()));
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
    if (p->domain.size()) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Domain:</b></td><td>");
      Strcat_charp(src, html_quote(p->domain.c_str()));
      Strcat_charp(src, "</td></tr>");
    }
    if (p->path.size()) {
      Strcat_charp(src, "<tr><td width=\"80\"><b>Path:</b></td><td>");
      Strcat_charp(src, html_quote(p->path.c_str()));
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
  return src->ptr;
}

void set_cookie_flag(struct keyvalue *arg) {
  while (arg) {
    if (arg->arg.size() && arg->value.size()) {
      int n = atoi(arg->arg.c_str());
      int v = atoi(arg->value.c_str());
      if (auto p = nth_cookie(n)) {
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
  // backBf({});
}

bool check_cookie_accept_domain(const char *domain) {
  if (!domain) {
    return false;
  }

  for (auto &tl : Cookie_accept_domains) {
    if (domain_match(domain, tl.c_str())) {
      return true;
    }
  }

  for (auto &tl : Cookie_reject_domains) {
    if (domain_match(domain, tl.c_str())) {
      return false;
    }
  }

  return true;
}

bool Cookie::match_cookie(const Url &pu, const char *domainname) const {
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
