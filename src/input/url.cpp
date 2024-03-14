#include "url.h"
#include "url_quote.h"
#include "myctype.h"
#include "enum_template.h"
#include "ioutil.h"
#include "quote.h"
#include <string.h>
#include <sstream>

enum CopyPathOption {
  COPYPATH_SPC_ALLOW = 0,
  COPYPATH_SPC_IGNORE = 1,
  COPYPATH_SPC_REPLACE = 2,
};
ENUM_OP_INSTANCE(CopyPathOption);
static std::string copyPath(const char *orgpath, int length,
                            CopyPathOption option,
                            bool COPYPATH_LOWERCASE = false) {
  std::stringstream ss;
  char ch;
  while ((ch = *orgpath) != 0 && length != 0) {
    if (COPYPATH_LOWERCASE) {
      ch = TOLOWER(ch);
    }
    if (IS_SPACE(ch)) {
      switch (option) {
      case COPYPATH_SPC_ALLOW:
        ss << ch;
        break;
      case COPYPATH_SPC_IGNORE:
        /* do nothing */
        break;
      case COPYPATH_SPC_REPLACE:
        ss << "%20";
        break;
      }
    } else {
      ss << ch;
    }
    orgpath++;
    length--;
  }
  return ss.str();
}

Url Url::parse(const char *__src, std::optional<Url> current) {
  // quote 0x01-0x20, 0x7F-0xFF
  auto _src = url_quote(__src);
  auto src = _src.c_str();
  auto p = src;
  const char *q = {};
  const char *qq = {};

  Url url = {};

  /* RFC1808: Relative Uniform Resource Locators
   * 4.  Resolving Relative URLs
   */
  if (*src == '\0' || *src == '#') {
    if (current) {
      url = *current;
    }
    url.do_label(p);
    return url;
  }

  /* search for scheme */
  auto scheme = parseUrlScheme(&p);
  if (scheme) {
    url.scheme = *scheme;
  } else {
    /* scheme part is not found in the url. This means either
     * (a) the url is relative to the current or (b) the url
     * denotes a filename (therefore the scheme is SCM_LOCAL).
     */
    if (current) {
      switch (current->scheme) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        url.scheme = SCM_LOCAL;
        break;

      default:
        url.scheme = current->scheme;
        break;
      }
    } else
      url.scheme = SCM_LOCAL;
    p = src;
    if (!strncmp(p, "//", 2)) {
      /* URL begins with // */
      /* it means that 'scheme:' is abbreviated */
      p += 2;
      goto analyze_url;
    }
    /* the url doesn't begin with '//' */
    goto analyze_file;
  }

  /* scheme part has been found */
  if (url.scheme == SCM_UNKNOWN) {
    url.file = src;
    return url;
  }

  /* get host and port */
  if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
    url.host = {};
    url.port = getDefaultPort(url.scheme);
    goto analyze_file;
  }

  /* after here, p begins with // */
  if (url.scheme == SCM_LOCAL) { /* file://foo           */
    if (p[2] == '/' || p[2] == '~'
    /* <A HREF="file:///foo">file:///foo</A>  or <A
     * HREF="file://~user">file://~user</A> */
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        || (IS_ALPHA(p[2]) && (p[3] == ':' || p[3] == '|'))
    /* <A HREF="file://DRIVE/foo">file://DRIVE/foo</A> */
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
    ) {
      p += 2;
      goto analyze_file;
    }
  }
  p += 2; /* scheme://foo         */
  /*          ^p is here  */

analyze_url:
  q = p;
#ifdef INET6
  if (*q == '[') { /* rfc2732,rfc2373 compliance */
    p++;
    while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
      p++;
    if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == nullptr))
      p = q;
  }
#endif
  while (*p && strchr(":/@?#", *p) == nullptr)
    p++;
  switch (*p) {
  case ':':
    /* scheme://user:pass@host or
     * scheme://host:port
     */
    qq = q;
    q = ++p;
    while (*p && strchr("@/?#", *p) == nullptr)
      p++;
    if (*p == '@') {
      /* scheme://user:pass@...       */
      url.user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
      url.pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
      p++;
      goto analyze_url;
    }
    /* scheme://host:port/ */
    url.host = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE, true);
    {
      std::string tmp(q, q + (p - q));
      url.port = atoi(tmp.c_str());
    }
    /* *p is one of ['\0', '/', '?', '#'] */
    break;
  case '@':
    /* scheme://user@...            */
    url.user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    p++;
    goto analyze_url;
  case '\0':
    /* scheme://host                */
  case '/':
  case '?':
  case '#':
    url.host = copyPath(q, p - q, COPYPATH_SPC_IGNORE, true);
    url.port = getDefaultPort(url.scheme);
    break;
  }
analyze_file:

  if ((*p == '\0' || *p == '#' || *p == '?') && url.host.empty()) {
    url.file = "";
    goto do_query;
  }

  q = p;
  if (*p == '/')
    p++;
  if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
    url.file = DefaultFile(url.scheme);
    goto do_query;
  }
  {
    auto cgi = strchr(p, '?');
  again:
    while (*p && *p != '#' && p != cgi)
      p++;
    if (*p == '#' && url.scheme == SCM_LOCAL) {
      /*
       * According to RFC2396, # means the beginning of
       * URI-reference, and # should be escaped.  But,
       * if the scheme is SCM_LOCAL, the special
       * treatment will apply to # for convinience.
       */
      if (p > q && *(p - 1) == '/' && (cgi == nullptr || p < cgi)) {
        /*
         * # comes as the first character of the file name
         * that means, # is not a label but a part of the file
         * name.
         */
        p++;
        goto again;
      } else if (*(p + 1) == '\0') {
        /*
         * # comes as the last character of the file name that
         * means, # is not a label but a part of the file
         * name.
         */
        p++;
      }
    }
    if (url.scheme == SCM_LOCAL) {
      url.file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    } else {
      url.file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    url.query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }

  url.do_label(p);

  return url;
}

void Url::do_label(std::string_view p) {
  // if (this->scheme == SCM_MISSING) {
  //   // ?
  //   assert(false);
  //   this->scheme = SCM_LOCAL;
  //   this->file = p;
  //   this->label = {};
  // } else
  if (p.size() && p[0] == '#') {
    this->label = p.substr(1);
  } else {
    this->label = {};
  }
}

std::string Url::to_Str(bool pass, bool user, bool label) const {

  // if (this->scheme == SCM_MISSING) {
  //   return "???";
  // }

  if (this->scheme == SCM_UNKNOWN) {
    return this->file.size() ? this->file : "";
  }

  if (this->host.empty() && this->file.empty() && label && this->label.size()) {
    /* local label */
    return std::string("#") + this->label;
  }

  if (this->scheme == SCM_LOCAL && this->file == "-") {
    std::string tmp("-");
    if (label && this->label.size()) {
      tmp += '#';
      tmp += this->label;
    }
    return tmp;
  }

  {
    std::stringstream tmp;
    tmp << scheme_str[this->scheme];
    tmp << ':';
    if (this->scheme == SCM_DATA) {
      tmp << this->file;
      return tmp.str();
    }

    tmp << "//";
    if (user && this->user.size()) {
      tmp << this->user;
      if (pass && this->pass.size()) {
        tmp << ':';
        tmp << this->pass;
      }
      tmp << '@';
    }
    if (this->host.size()) {
      tmp << this->host;
      if (this->port != getDefaultPort(this->scheme)) {
        tmp << ':';
        tmp << this->port;
      }
    }

    if ((this->file.empty() ||
         (this->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
          && !(IS_ALPHA(this->file[0]) && this->file[1] == ':' &&
               this->host == nullptr)
#endif
              ))) {
      tmp << '/';
    }
    tmp << this->file;
    if (this->query.size()) {
      tmp << '?';
      tmp << this->query;
    }
    if (label && this->label.size()) {
      tmp << '#';
      tmp << this->label;
    }
    return tmp.str();
  }
}

Url urlParse(const char *src, std::optional<Url> current) {
  auto url = Url::parse(src, current);
  if (url.scheme == SCM_DATA) {
    return url;
  }

  if (url.scheme == SCM_LOCAL) {
    auto q = ioutil::expandName(ioutil::file_unquote(url.file));
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      std::string drive = q.substr(0, 2);
      drive += ioutil::file_quote(q.substr(2));
      url.file = drive;
    } else {
      url.file = ioutil::file_quote(q);
    }
  }

  int relative_uri = false;
  if (current &&
      (url.scheme == current->scheme ||
       (url.scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) &&
      url.host.empty()) {
    /* Copy omitted element from the current URL */
    url.user = current->user;
    url.pass = current->pass;
    url.host = current->host;
    url.port = current->port;
    if (url.file.size()) {
      if (url.file[0] != '/' &&
          !(url.scheme == SCM_LOCAL && IS_ALPHA(url.file[0]) &&
            url.file[1] == ':')) {
        /* file is relative [process 1] */
        auto p = url.file;
        if (current->file.size()) {
          std::string tmp = current->file;
          while (tmp.size()) {
            if (tmp.back() == '/') {
              break;
            }
            tmp.pop_back();
          }
          tmp += p;
          url.file = tmp;
          relative_uri = true;
        }
      }
    } else { /* scheme:[?query][#label] */
      url.file = current->file;
      if (url.query.empty()) {
        url.query = current->query;
      }
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }

  if (url.file.size()) {
    if (url.scheme == SCM_LOCAL && url.file[0] != '/' &&
        !(IS_ALPHA(url.file[0]) && url.file[1] == ':') && url.file != "-") {
      /* local file, relative path */
      auto tmp = ioutil::pwd();
      if (tmp.back() != '/') {
        tmp += '/';
      }
      tmp += ioutil::file_unquote(url.file.c_str());
      url.file = ioutil::file_quote(cleanupName(tmp.c_str()));
    } else if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, url.file is created by [process 1] above.
         * url.file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the url.file. It is
         * server's responsibility to canonicalize such path.
         */
        url.file = cleanupName(url.file.c_str());
      }
    } else if (url.file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp scheme (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      url.file = cleanupName(url.file.c_str());
    }
    if (url.scheme == SCM_LOCAL) {
      if (url.host.size() && !ioutil::is_localhost(url.host)) {
        std::string tmp("//");
        tmp += url.host;
        tmp += cleanupName(ioutil::file_unquote(url.file.c_str()));
        url.real_file = tmp;
      } else {
        url.real_file = cleanupName(ioutil::file_unquote(url.file.c_str()));
      }
    }
  }
  return url;
}
