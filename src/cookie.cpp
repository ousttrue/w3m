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
#include "cmp.h"
#include "myctype.h"
#include "cookie_domain.h"

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <span>
#include <time.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <memory>
#ifndef _MSC_VER
#include <sys/stat.h>
#endif

struct Cookie {
  Url url = {};
  std::string name;
  std::string value;
  time_t expires = {};
  std::string path;
  std::string domain;
  std::string comment = {};
  std::string commentURL = {};
  std::vector<uint16_t> portl;
  char version = {};
  CookieFlags flag = {};

  std::shared_ptr<Cookie> next;

  std::string make_cookie() const { return name + '=' + value; }
  bool match_cookie(const Url &pu, std::string_view domainname) const;
};

static std::string readcol(char **p) {
  std::stringstream tmp;
  while (**p && **p != '\n' && **p != '\r' && **p != '\t')
    tmp << *((*p)++);
  if (**p == '\t')
    (*p)++;
  return tmp.str();
}

static std::string portlist2str(std::span<uint16_t> list) {
  std::stringstream tmp;
  for (size_t i = 0; i < list.size(); ++i) {
    if (i) {
      tmp << ", ";
    }
    tmp << list[i];
  }
  return tmp.str();
}

static std::vector<uint16_t> make_portlist(std::string_view port) {
  std::vector<uint16_t> list;
  auto p = port.begin();
  while (p != port.end()) {
    while (p != port.end() && !IS_DIGIT(*p))
      p++;
    std::string tmp;
    while (*p && IS_DIGIT(*p))
      tmp += *(p++);
    if (tmp.empty()) {
      break;
    }
    list.push_back(atoi(tmp.c_str()));
  }
  return list;
}

class CookieManager {
  std::shared_ptr<Cookie> First_cookie;
  bool is_saved = true;

  CookieManager() {}

public:
  CookieManager(const CookieManager &) = delete;
  CookieManager &operator=(const CookieManager &) = delete;
  ~CookieManager() {}

  static CookieManager &instance() {
    static CookieManager s_instance;
    return s_instance;
  }

  void save(const std::string &cookie_file) {
    check_expired_cookies();

    if (!First_cookie || is_saved || no_rc_dir)
      return;

    FILE *fp;
    if (!(fp = fopen(cookie_file.c_str(), "w")))
      return;

    for (auto p = First_cookie; p; p = p->next) {
      if (!(p->flag & COO_USE) || p->flag & COO_DISCARD)
        continue;
      fprintf(fp, "%s\t%s\t%s\t%ld\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n",
              p->url.to_Str().c_str(), p->name.c_str(), p->value.c_str(),
              (long)p->expires, p->domain.c_str(), p->path.c_str(), p->flag,
              p->version, p->comment.c_str(), portlist2str(p->portl).c_str(),
              p->commentURL.c_str());
    }
    fclose(fp);

#ifndef _MSC_VER
    chmod(cookie_file.c_str(), S_IRUSR | S_IWUSR);
#endif
  }

  void load(const std::string &cookie_file) {
    std::ifstream fp(expandPath(cookie_file), std::ios::binary);
    if (!fp) {
      return;
    }

    std::shared_ptr<Cookie> p;
    if (First_cookie) {
      for (p = First_cookie; p->next; p = p->next)
        ;
    };

    std::string line;
    while (std::getline(fp, line)) {
      auto str = line.data();
      auto cookie = std::make_shared<Cookie>();
      cookie->next = NULL;
      cookie->flag = {};
      cookie->version = 0;
      cookie->expires = (time_t)-1;
      cookie->comment = {};
      cookie->portl = {};
      cookie->commentURL = {};
      cookie->url = {readcol(&str)};
      if (!*str)
        break;
      cookie->name = readcol(&str);
      if (!*str)
        break;
      cookie->value = readcol(&str);
      if (!*str)
        break;
      cookie->expires = (time_t)atol(readcol(&str).c_str());
      if (!*str)
        break;
      cookie->domain = readcol(&str);
      if (!*str)
        break;
      cookie->path = readcol(&str);
      if (!*str)
        break;
      cookie->flag = (CookieFlags)atoi(readcol(&str).c_str());
      if (!*str)
        break;
      cookie->version = atoi(readcol(&str).c_str());
      if (!*str)
        break;
      cookie->comment = readcol(&str);
      if (!*str)
        break;
      cookie->portl = make_portlist(readcol(&str));
      if (!*str)
        break;
      cookie->commentURL = readcol(&str);

      if (p)
        p->next = cookie;
      else
        First_cookie = cookie;
      p = cookie;
    }

    check_expired_cookies();
  }

  void check_expired_cookies() {
    time_t now = time(NULL);

    if (!First_cookie)
      return;

    if (First_cookie->expires != (time_t)-1 && First_cookie->expires < now) {
      if (!(First_cookie->flag & COO_DISCARD))
        is_saved = 0;
      First_cookie = First_cookie->next;
    }

    std::shared_ptr<Cookie> p1;
    for (auto p = First_cookie; p && p->next; p = p1) {
      p1 = p->next;
      if (p1->expires != (time_t)-1 && p1->expires < now) {
        if (!(p1->flag & COO_DISCARD))
          is_saved = 0;
        p->next = p1->next;
        p1 = p;
      }
    }
  }

  std::optional<std::string> find_cookie(const Url &pu) {
    int version = 0;

    std::shared_ptr<Cookie> p1;
    std::shared_ptr<Cookie> fco;
    auto fq_domainname = FQDN(pu.host);
    check_expired_cookies();
    for (auto p = First_cookie; p; p = p->next) {
      auto domainname = (p->version == 0) ? fq_domainname : pu.host;
      if (p->flag & COO_USE && p->match_cookie(pu, domainname)) {
        for (auto p1 = fco; p1 && p1->name != p->name; p1 = p1->next)
          ;
        if (p1)
          continue;
        p1 = std::make_shared<Cookie>();
        *p1 = *p;
        p1->next = fco;
        fco = p1;
        if (p1->version > version)
          version = p1->version;
      }
    }

    if (!fco)
      return {};

    std::stringstream tmp;
    if (version > 0)
      tmp << "$Version=\"" << version << "\"; ";

    tmp << fco->make_cookie();
    for (p1 = fco->next; p1; p1 = p1->next) {
      tmp << "; ";
      tmp << p1->make_cookie();
      if (version > 0) {
        if (p1->flag & COO_PATH)
          tmp << "; $Path=\"" << p1->path << "\"";
        if (p1->flag & COO_DOMAIN)
          tmp << "; $Domain=\"" << p1->domain << "\"";
        if (p1->portl.size())
          tmp << "; $Port=\"" << portlist2str(p1->portl) << "\"";
      }
    }
    return tmp.str();
  }

  std::shared_ptr<Cookie> get_cookie_info(std::string_view domain,
                                          std::string_view path,
                                          std::string_view name) {
    for (auto p = First_cookie; p; p = p->next) {
      if (p->domain == domain && p->path == path && p->name == name)
        return p;
    }
    return {};
  }

  std::shared_ptr<Cookie> getOrCreate(std::string_view domain,
                                      std::string_view path,
                                      std::string_view name) {
    auto p = get_cookie_info(domain, path, name);
    if (!p) {
      p = std::make_shared<Cookie>();
      p->flag = {};
      if (default_use_cookie) {
        p->flag = (CookieFlags)(p->flag | COO_USE);
      }
      p->next = First_cookie;
      First_cookie = p;
    }
    return p;
  }

  void updateFlag(const std::shared_ptr<Cookie> &p, const std::string &odomain,
                  const std::string &opath, CookieFlags flag) {
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
  }

  std::shared_ptr<Cookie> nth_cookie(int n) {
    int i = 0;
    for (auto p = First_cookie; p; p = p->next, i++) {
      if (i == n)
        return p;
    }
    return {};
  }

  std::string panel() const {
    std::stringstream src;
    src << "<html><head><title>Cookies</title></head>"
           "<body><center><b>Cookies</b></center>"
           "<p><form method=internal action=cookie>";

    if (!use_cookie || !First_cookie)
      return NULL;

    src << "<ol>";
    int i = 0;
    for (auto p = First_cookie; p; p = p->next, i++) {
      auto tmp = html_quote(p->url.to_Str());
      char tmp2[80];
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
                   (gmt->tm_year) + 1900, gmt->tm_hour, gmt->tm_min,
                   gmt->tm_sec);
        } else {
          snprintf(&tmp2[11], sizeof(tmp2) - 11, "%04d %02d:%02d:%02d GMT",
                   gmt->tm_year, gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
        }
#endif /* not HAVE_STRFTIME */
      } else {
        tmp2[0] = '\0';
      }

      src << "<li>";
      src << "<h1><a href=\"";
      src << tmp;
      src << "\">";
      src << tmp;
      src << "</a></h1>";

      src << "<table cellpadding=0>";
      if (!(p->flag & COO_SECURE)) {
        src << "<tr><td width=\"80\"><b>Cookie:</b></td><td>";
        src << html_quote(p->make_cookie());
        src << "</td></tr>";
      }
      if (p->comment.size()) {
        src << "<tr><td width=\"80\"><b>Comment:</b></td><td>";
        src << html_quote(p->comment);
        src << "</td></tr>";
      }
      if (p->commentURL.size()) {
        src << "<tr><td width=\"80\"><b>CommentURL:</b></td><td>";
        src << "<a href=\"";
        src << html_quote(p->commentURL);
        src << "\">";
        src << html_quote(p->commentURL);
        src << "</a>";
        src << "</td></tr>";
      }
      if (tmp2[0]) {
        src << "<tr><td width=\"80\"><b>Expires:</b></td><td>";
        src << tmp2;
        if (p->flag & COO_DISCARD) {
          src << " (Discard)";
        }
        src << "</td></tr>";
      }
      src << "<tr><td width=\"80\"><b>Version:</b></td><td>";
      src << p->version;
      src << "</td></tr><tr><td>";
      if (p->domain.size()) {
        src << "<tr><td width=\"80\"><b>Domain:</b></td><td>";
        src << html_quote(p->domain);
        src << "</td></tr>";
      }
      if (p->path.size()) {
        src << "<tr><td width=\"80\"><b>Path:</b></td><td>";
        src << html_quote(p->path);
        src << "</td></tr>";
      }
      if (p->portl.size()) {
        src << "<tr><td width=\"80\"><b>Port:</b></td><td>";
        src << html_quote(portlist2str(p->portl));
        src << "</td></tr>";
      }
      src << "<tr><td width=\"80\"><b>Secure:</b></td><td>";
      src << (p->flag & COO_SECURE ? "Yes" : "No");
      src << "</td></tr><tr><td>";

      src << "<tr><td width=\"80\"><b>Use:</b></td><td>"
             "<input type=radio name=\""
          << i << "\" value=1" << ((p->flag & COO_USE) ? " checked" : "")
          << ">Yes"
             "&nbsp;&nbsp;"
             "<input type=radio name=\""
          << i << "\" value=0" << ((!(p->flag & COO_USE)) ? " checked" : "")
          << ">No";

      src << "</td></tr><tr><td><input type=submit "
             "value=\"OK\"></table><p>";
    }
    src << "</ol></form></body></html>";
    return src.str();
  }

  void setFlag(const std::list<std::pair<std::string, std::string>> &list) {
    for (auto [key, value] : list) {
      if (key.size() && value.size()) {
        int n = atoi(key.c_str());
        int v = atoi(value.c_str());
        if (auto p = nth_cookie(n)) {
          if (v && !(p->flag & COO_USE))
            p->flag = (CookieFlags)(p->flag | COO_USE);
          else if (!v && p->flag & COO_USE)
            p->flag = (CookieFlags)(p->flag & ~COO_USE);
          if (!(p->flag & COO_DISCARD))
            is_saved = 0;
        }
      }
    }
  }
};

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

bool no_rc_dir = false;
bool default_use_cookie = true;
bool use_cookie = true;
bool show_cookie = false;
bool accept_cookie = true;
int accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;

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

std::optional<std::string> find_cookie(const Url &pu) {
  return CookieManager::instance().find_cookie(pu);
}

int add_cookie(const Url *pu, std::string_view name, std::string_view value,
               time_t expires, std::string_view domain, std::string_view _path,
               CookieFlags flag, std::string_view comment, int version,
               std::string_view port, std::string_view commentURL) {
  std::string domainname = (version == 0) ? FQDN(pu->host.c_str()) : pu->host;
  std::string odomain(domain.begin(), domain.end());
  std::string path(_path.begin(), _path.end());
  std::string opath = path;
  int use_security = !(flag & COO_OVERRIDE);

#define COOKIE_ERROR(err)                                                      \
  if (!((err) & COO_OVERRIDE_OK) || use_security)                              \
  return (err)

  /* [RFC 2109] s. 4.3.2 case 2; but this (no request-host) shouldn't happen
   */
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

  std::vector<uint16_t> portlist;
  if (port.size()) {
    /* [DRAFT 12] s. 4.3.2 case 5 */
    portlist = make_portlist(port);
    if (std::find(portlist.begin(), portlist.end(), pu->port) ==
        portlist.end()) {
      // not found
      COOKIE_ERROR(COO_EPORT);
    }
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

  auto p = CookieManager::instance().getOrCreate(domain, path, name);

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

  CookieManager::instance().updateFlag(p, odomain, opath, flag);

  return 0;
}

void save_cookies(const std::string &cookie_file) {
  CookieManager::instance().save(cookie_file);
}

void load_cookies(const std::string &cookie_file) {
  CookieManager::instance().load(cookie_file);
}

std::string cookie_list_panel() { return CookieManager::instance().panel(); }

void set_cookie_flag(
    const std::list<std::pair<std::string, std::string>> &arg) {
  CookieManager::instance().setFlag(arg);
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

bool Cookie::match_cookie(const Url &pu, std::string_view domainname) const {
  if (domainname.empty()) {
    return 0;
  }
  if (!domain_match(domainname, this->domain.c_str()))
    return 0;
  if (!pu.file.starts_with(this->path))
    return 0;
  if (this->flag & COO_SECURE && pu.scheme != SCM_HTTPS)
    return 0;
  if (this->portl.size() &&
      std::find(portl.begin(), portl.end(), pu.port) == portl.end())
    return 0;

  return 1;
}
