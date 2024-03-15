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

void Url::parse(const char *__src, std::optional<Url> current) {
  // quote 0x01-0x20, 0x7F-0xFF
  auto _src = url_quote(__src);
  auto src = _src.c_str();
  auto p = src;
  const char *q = {};
  const char *qq = {};

  /* RFC1808: Relative Uniform Resource Locators
   * 4.  Resolving Relative URLs
   */
  if (*src == '\0' || *src == '#') {
    if (current) {
      *this = *current;
    }
    this->do_label(p);
    return;
  }

  /* search for scheme */
  auto scheme = parseUrlScheme(&p);
  if (scheme) {
    this->scheme = *scheme;
  } else {
    /* scheme part is not found in the url. This means either
     * (a) the url is relative to the current or (b) the url
     * denotes a filename (therefore the scheme is SCM_LOCAL).
     */
    if (current) {
      switch (current->scheme) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        this->scheme = SCM_LOCAL;
        break;

      default:
        this->scheme = current->scheme;
        break;
      }
    } else {
      this->scheme = SCM_LOCAL;
    }
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
  if (this->scheme == SCM_UNKNOWN) {
    this->file = src;
    return;
  }

  /* get host and port */
  if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
    this->host = {};
    this->port = getDefaultPort(this->scheme);
    goto analyze_file;
  }

  /* after here, p begins with // */
  if (this->scheme == SCM_LOCAL) { /* file://foo           */
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
      this->user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
      this->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
      p++;
      goto analyze_url;
    }
    /* scheme://host:port/ */
    this->host = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE, true);
    {
      std::string tmp(q, q + (p - q));
      this->port = atoi(tmp.c_str());
    }
    /* *p is one of ['\0', '/', '?', '#'] */
    break;

  case '@':
    /* scheme://user@...            */
    this->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    p++;
    goto analyze_url;

  case '\0':
    /* scheme://host                */
  case '/':
  case '?':
  case '#':
    this->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE, true);
    this->port = getDefaultPort(this->scheme);
    break;
  }

analyze_file:
  if ((*p == '\0' || *p == '#' || *p == '?') && this->host.empty()) {
    this->file = "";
    goto do_query;
  }

  q = p;
  if (*p == '/')
    p++;
  if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
    this->file = DefaultFile(this->scheme);
    goto do_query;
  }

  {
    auto cgi = strchr(p, '?');
  again:
    while (*p && *p != '#' && p != cgi)
      p++;
    if (*p == '#' && this->scheme == SCM_LOCAL) {
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
    if (this->scheme == SCM_LOCAL) {
      this->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    } else {
      this->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    this->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }

  this->do_label(p);
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

Url::Url(const std::string &_src, std::optional<Url> current) {
  auto src = _src.c_str();
  this->parse(src, current);
  if (this->scheme == SCM_DATA) {
    return;
  }

  if (this->scheme == SCM_LOCAL) {
    auto q = ioutil::expandName(ioutil::file_unquote(this->file));
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      std::string drive = q.substr(0, 2);
      drive += ioutil::file_quote(q.substr(2));
      this->file = drive;
    } else {
      this->file = ioutil::file_quote(q);
    }
  }

  int relative_uri = false;
  if (current &&
      (this->scheme == current->scheme ||
       (this->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) &&
      this->host.empty()) {
    /* Copy omitted element from the current URL */
    this->user = current->user;
    this->pass = current->pass;
    this->host = current->host;
    this->port = current->port;
    if (this->file.size()) {
      if (this->file[0] != '/' &&
          !(this->scheme == SCM_LOCAL && IS_ALPHA(this->file[0]) &&
            this->file[1] == ':')) {
        /* file is relative [process 1] */
        auto p = this->file;
        if (current->file.size()) {
          std::string tmp = current->file;
          while (tmp.size()) {
            if (tmp.back() == '/') {
              break;
            }
            tmp.pop_back();
          }
          tmp += p;
          this->file = tmp;
          relative_uri = true;
        }
      }
    } else { /* scheme:[?query][#label] */
      this->file = current->file;
      if (this->query.empty()) {
        this->query = current->query;
      }
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }

  if (this->file.size()) {
    if (this->scheme == SCM_LOCAL && this->file[0] != '/' &&
        !(IS_ALPHA(this->file[0]) && this->file[1] == ':') &&
        this->file != "-") {
      /* local file, relative path */
      auto tmp = ioutil::pwd();
      if (tmp.back() != '/') {
        tmp += '/';
      }
      tmp += ioutil::file_unquote(this->file.c_str());
      this->file = ioutil::file_quote(cleanupName(tmp.c_str()));
    } else if (this->scheme == SCM_HTTP || this->scheme == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, this->file is created by [process 1] above.
         * this->file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the this->file. It is
         * server's responsibility to canonicalize such path.
         */
        this->file = cleanupName(this->file.c_str());
      }
    } else if (this->file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp scheme (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      this->file = cleanupName(this->file.c_str());
    }
    if (this->scheme == SCM_LOCAL) {
      if (this->host.size() && !ioutil::is_localhost(this->host)) {
        std::string tmp("//");
        tmp += this->host;
        tmp += cleanupName(ioutil::file_unquote(this->file.c_str()));
        this->real_file = tmp;
      } else {
        this->real_file = cleanupName(ioutil::file_unquote(this->file.c_str()));
      }
    }
  }
}
