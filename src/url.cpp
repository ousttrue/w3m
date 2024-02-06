#include "w3m.h"
#include "url_stream.h"
#include "etc.h"
#include "myctype.h"
#include "enum_template.h"
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

Url Url::parse(const char *src, std::optional<Url> current) {
  // quote 0x01-0x20, 0x7F-0xFF
  src = url_quote(src);
  auto p = src;
  const char *q = {};
  const char *qq = {};

  Url url = {.schema = SCM_MISSING};

  /* RFC1808: Relative Uniform Resource Locators
   * 4.  Resolving Relative URLs
   */
  if (*src == '\0' || *src == '#') {
    if (current) {
      url = *current;
    }
    goto do_label;
  }

  /* search for schema */
  url.schema = parseUrlSchema(&p);
  if (url.schema == SCM_MISSING) {
    /* schema part is not found in the url. This means either
     * (a) the url is relative to the current or (b) the url
     * denotes a filename (therefore the schema is SCM_LOCAL).
     */
    if (current) {
      switch (current->schema) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        url.schema = SCM_LOCAL;
        break;

      default:
        url.schema = current->schema;
        break;
      }
    } else
      url.schema = SCM_LOCAL;
    p = src;
    if (!strncmp(p, "//", 2)) {
      /* URL begins with // */
      /* it means that 'schema:' is abbreviated */
      p += 2;
      goto analyze_url;
    }
    /* the url doesn't begin with '//' */
    goto analyze_file;
  }

  /* schema part has been found */
  if (url.schema == SCM_UNKNOWN) {
    url.file = src;
    return url;
  }

  /* get host and port */
  if (p[0] != '/' || p[1] != '/') { /* schema:foo or schema:/foo */
    url.host = nullptr;
    url.port = getDefaultPort(url.schema);
    goto analyze_file;
  }

  /* after here, p begins with // */
  if (url.schema == SCM_LOCAL) { /* file://foo           */
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
  p += 2; /* schema://foo         */
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
    /* schema://user:pass@host or
     * schema://host:port
     */
    qq = q;
    q = ++p;
    while (*p && strchr("@/?#", *p) == nullptr)
      p++;
    if (*p == '@') {
      /* schema://user:pass@...       */
      url.user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
      url.pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
      p++;
      goto analyze_url;
    }
    /* schema://host:port/ */
    url.host = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE, true);
    {
      std::string tmp(q, q + (p - q));
      url.port = atoi(tmp.c_str());
    }
    /* *p is one of ['\0', '/', '?', '#'] */
    break;
  case '@':
    /* schema://user@...            */
    url.user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    p++;
    goto analyze_url;
  case '\0':
    /* schema://host                */
  case '/':
  case '?':
  case '#':
    url.host = copyPath(q, p - q, COPYPATH_SPC_IGNORE, true);
    url.port = getDefaultPort(url.schema);
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
  if (*p == '\0' || *p == '#' || *p == '?') { /* schema://host[:port]/ */
    url.file = DefaultFile(url.schema);
    goto do_query;
  }
  {
    auto cgi = strchr(p, '?');
  again:
    while (*p && *p != '#' && p != cgi)
      p++;
    if (*p == '#' && url.schema == SCM_LOCAL) {
      /*
       * According to RFC2396, # means the beginning of
       * URI-reference, and # should be escaped.  But,
       * if the schema is SCM_LOCAL, the special
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
    if (url.schema == SCM_LOCAL || url.schema == SCM_MISSING)
      url.file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    else
      url.file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    url.query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }
do_label:
  if (url.schema == SCM_MISSING) {
    url.schema = SCM_LOCAL;
    url.file = p;
    url.label = {};
  } else if (*p == '#')
    url.label = p + 1;
  else
    url.label = {};

  return url;
}

Url Url::parse2(const char *src, std::optional<Url> current) {
  auto url = Url::parse(src, current);
  if (url.schema == SCM_DATA) {
    return url;
  }

  if (url.schema == SCM_LOCAL) {
    auto q = expandName(file_unquote(url.file.c_str()));
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      std::string drive(q, q + 2);
      drive += file_quote(q + 2);
      url.file = drive;
    } else {
      url.file = file_quote(q);
    }
  }

  int relative_uri = false;
  if (current &&
      (url.schema == current->schema ||
       (url.schema == SCM_LOCAL && current->schema == SCM_LOCAL_CGI)) &&
      url.host.empty()) {
    /* Copy omitted element from the current URL */
    url.user = current->user;
    url.pass = current->pass;
    url.host = current->host;
    url.port = current->port;
    if (url.file.size()) {
      if (url.file[0] != '/' &&
          !(url.schema == SCM_LOCAL && IS_ALPHA(url.file[0]) &&
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
    } else { /* schema:[?query][#label] */
      url.file = current->file;
      if (url.query.empty()) {
        url.query = current->query;
      }
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }

  if (url.file.size()) {
    if (url.schema == SCM_LOCAL && url.file[0] != '/' &&
        !(IS_ALPHA(url.file[0]) && url.file[1] == ':') && url.file != "-") {
      /* local file, relative path */
      std::string tmp = CurrentDir;
      if (tmp.back() != '/') {
        tmp += '/';
      }
      tmp += file_unquote(url.file.c_str());
      url.file = file_quote(cleanupName(tmp.c_str()));
    } else if (url.schema == SCM_HTTP || url.schema == SCM_HTTPS) {
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
       * (1) ftp schema (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      url.file = cleanupName(url.file.c_str());
    }
    if (url.schema == SCM_LOCAL) {
      if (url.host.size() && !is_localhost(url.host.c_str())) {
        std::string tmp("//");
        tmp += url.host;
        tmp += cleanupName(file_unquote(url.file.c_str()));
        url.real_file = tmp;
      } else {
        url.real_file = cleanupName(file_unquote(url.file.c_str()));
      }
    }
  }
  return url;
}

std::string Url::to_Str(bool pass, bool user, bool label) const {

  if (this->schema == SCM_MISSING) {
    return "???";
  }

  if (this->schema == SCM_UNKNOWN) {
    return this->file.size() ? this->file : "";
  }

  if (this->host.empty() && this->file.empty() && label && this->label.size()) {
    /* local label */
    return std::string("#") + this->label;
  }

  if (this->schema == SCM_LOCAL && this->file == "-") {
    std::string tmp("-");
    if (label && this->label.size()) {
      tmp += '#';
      tmp += this->label;
    }
    return tmp;
  }

  {
    std::stringstream tmp;
    tmp << schema_str[this->schema];
    tmp << ':';
    if (this->schema == SCM_DATA) {
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
      if (this->port != getDefaultPort(this->schema)) {
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
