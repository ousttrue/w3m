#include "w3m.h"
#include "quote.h"
#include "contentinfo.h"
#include "etc.h"
#include "message.h"
#include "ssl_util.h"
#include "screen.h"
#include "display.h"
#include "terms.h"
#include "httprequest.h"
#include "rc.h"
#include "cookie.h"
#include "buffer.h"
#include "textlist.h"
#include "istream.h"
#include "form.h"
#include "signal_util.h"
#include "proto.h"
#include "html.h"
#include "Str.h"
#include "myctype.h"
#include "regex.h"
#include <vector>

bool ArgvIsURL = true;
bool LocalhostOnly = false;
bool retryAsHttp = true;

#define ALLOC_STR(s) ((s) == nullptr ? nullptr : allocStr(s, -1))
Url &Url::operator=(const Url &src) {
  this->schema = src.schema;
  this->port = src.port;
  this->is_nocache = src.is_nocache;
  this->user = ALLOC_STR(src.user);
  this->pass = ALLOC_STR(src.pass);
  this->host = ALLOC_STR(src.host);
  this->file = ALLOC_STR(src.file);
  this->real_file = ALLOC_STR(src.real_file);
  this->label = ALLOC_STR(src.label);
  this->query = ALLOC_STR(src.query);
  return *this;
}

Url *baseURL(Buffer *buf) {
  if (buf->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return nullptr;
  }
  if (buf->info->baseURL) {
    /* <BASE> tag is defined in the document */
    return &*buf->info->baseURL;
  } else if (buf->info->currentURL.IS_EMPTY_PARSED_URL())
    return nullptr;
  else
    return &buf->info->currentURL;
}

#define COPYPATH_SPC_ALLOW 0
#define COPYPATH_SPC_IGNORE 1
#define COPYPATH_SPC_REPLACE 2
#define COPYPATH_SPC_MASK 3
#define COPYPATH_LOWERCASE 4

static const char *copyPath(const char *orgpath, int length, int option) {
  Str *tmp = Strnew();
  char ch;
  while ((ch = *orgpath) != 0 && length != 0) {
    if (option & COPYPATH_LOWERCASE)
      ch = TOLOWER(ch);
    if (IS_SPACE(ch)) {
      switch (option & COPYPATH_SPC_MASK) {
      case COPYPATH_SPC_ALLOW:
        Strcat_char(tmp, ch);
        break;
      case COPYPATH_SPC_IGNORE:
        /* do nothing */
        break;
      case COPYPATH_SPC_REPLACE:
        Strcat_charp(tmp, "%20");
        break;
      }
    } else
      Strcat_char(tmp, ch);
    orgpath++;
    length--;
  }
  return tmp->ptr;
}

Url Url::parse(const char *src, const Url *current) {
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
      case SCM_FTP:
      case SCM_FTPDIR:
        url.schema = SCM_FTP;
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
    url.file = allocStr(src, -1);
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
    url.host =
        copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    {
      auto tmp = Strnew_charp_n(q, p - q);
      url.port = atoi(tmp->ptr);
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
    url.host = copyPath(q, p - q, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    url.port = getDefaultPort(url.schema);
    break;
  }
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
  if (url.schema == SCM_LOCAL && url.user == nullptr && url.host != nullptr &&
      *url.host != '\0' && !is_localhost(url.host)) {
    /*
     * In the environments other than CYGWIN, a URL like
     * file://host/file is regarded as ftp://host/file.
     * On the other hand, file://host/file on CYGWIN is
     * regarded as local access to the file //host/file.
     * `host' is a netbios-hostname, drive, or any other
     * name; It is CYGWIN system call who interprets that.
     */

    url.schema = SCM_FTP; /* ftp://host/... */
    if (url.port == 0)
      url.port = getDefaultPort(SCM_FTP);
  }
#endif
  if ((*p == '\0' || *p == '#' || *p == '?') && url.host == nullptr) {
    url.file = "";
    goto do_query;
  }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (url.schema == SCM_LOCAL) {
    q = p;
    if (*q == '/')
      q++;
    if (IS_ALPHA(q[0]) && (q[1] == ':' || q[1] == '|')) {
      if (q[1] == '|') {
        p = allocStr(q, -1);
        p[1] = ':';
      } else
        p = q;
    }
  }
#endif

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
      url.file = (char *)copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    else
      url.file = (char *)copyPath(q, p - q, COPYPATH_SPC_IGNORE);
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    url.query = (char *)copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }
do_label:
  if (url.schema == SCM_MISSING) {
    url.schema = SCM_LOCAL;
    url.file = allocStr(p, -1);
    url.label = nullptr;
  } else if (*p == '#')
    url.label = allocStr(p + 1, -1);
  else
    url.label = nullptr;

  return url;
}

Url Url::parse2(const char *src, const Url *current) {
  auto url = Url::parse(src, current);
  if (url.schema == SCM_MAILTO) {
    return url;
  }
  if (url.schema == SCM_DATA) {
    return url;
  }

  if (url.schema == SCM_NEWS || url.schema == SCM_NEWS_GROUP) {
    const char *p;
    if (url.file && !strchr(url.file, '@') &&
        (!(p = strchr(url.file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      url.schema = SCM_NEWS_GROUP;
    else
      url.schema = SCM_NEWS;
    return url;
  }

  if (url.schema == SCM_NNTP || url.schema == SCM_NNTP_GROUP) {
    if (url.file && *url.file == '/')
      url.file = allocStr(url.file + 1, -1);
    const char *p;
    if (url.file && !strchr(url.file, '@') &&
        (!(p = strchr(url.file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      url.schema = SCM_NNTP_GROUP;
    else
      url.schema = SCM_NNTP;
    if (current &&
        (current->schema == SCM_NNTP || current->schema == SCM_NNTP_GROUP)) {
      if (url.host == nullptr) {
        url.host = current->host;
        url.port = current->port;
      }
    }
    return url;
  }

  if (url.schema == SCM_LOCAL) {
    auto q = expandName(file_unquote((char *)url.file));
    Str *drive;
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      drive = Strnew_charp_n(q, 2);
      Strcat_charp(drive, file_quote(q + 2));
      url.file = drive->ptr;
    } else {
      url.file = file_quote((char *)q);
    }
  }

  int relative_uri = false;
  if (current &&
      (url.schema == current->schema ||
       (url.schema == SCM_FTP && current->schema == SCM_FTPDIR) ||
       (url.schema == SCM_LOCAL && current->schema == SCM_LOCAL_CGI)) &&
      url.host == nullptr) {
    /* Copy omitted element from the current URL */
    url.user = current->user;
    url.pass = current->pass;
    url.host = current->host;
    url.port = current->port;
    if (url.file && *url.file) {
      if (url.file[0] != '/' &&
          !(url.schema == SCM_LOCAL && IS_ALPHA(url.file[0]) &&
            url.file[1] == ':')) {
        /* file is relative [process 1] */
        auto p = url.file;
        if (current->file) {
          auto tmp = Strnew_charp(current->file);
          while (tmp->length > 0) {
            if (Strlastchar(tmp) == '/')
              break;
            Strshrink(tmp, 1);
          }
          Strcat_charp(tmp, p);
          url.file = tmp->ptr;
          relative_uri = true;
        }
      }
    } else { /* schema:[?query][#label] */
      url.file = current->file;
      if (!url.query)
        url.query = current->query;
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }

  if (url.file) {
    if (url.schema == SCM_LOCAL && url.file[0] != '/' &&
        !(IS_ALPHA(url.file[0]) && url.file[1] == ':') &&
        strcmp(url.file, "-")) {
      /* local file, relative path */
      auto tmp = Strnew_charp(CurrentDir);
      if (Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
      Strcat_charp(tmp, file_unquote((char *)url.file));
      url.file = file_quote(cleanupName(tmp->ptr));
    } else if (url.schema == SCM_HTTP || url.schema == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, url.file is created by [process 1] above.
         * url.file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the url.file. It is
         * server's responsibility to canonicalize such path.
         */
        url.file = cleanupName((char *)url.file);
      }
    } else if (url.file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp schema (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      url.file = cleanupName((char *)url.file);
    }
    if (url.schema == SCM_LOCAL) {
      if (url.host && !is_localhost(url.host)) {
        Str *tmp = Strnew_charp("//");
        Strcat_m_charp(tmp, url.host, cleanupName(file_unquote(url.file)),
                       nullptr);
        url.real_file = tmp->ptr;
      } else {
        url.real_file = cleanupName(file_unquote(url.file));
      }
    }
  }
  return url;
}

std::string Url::to_Str(bool pass, bool user, bool label) const {
  Str *tmp;
  static const char *schema_str[] = {
      "http", "gopher", "ftp",  "ftp",  "file", "file",   "exec",
      "nntp", "nntp",   "news", "news", "data", "mailto", "https",
  };

  if (this->schema == SCM_MISSING) {
    return "???";
  } else if (this->schema == SCM_UNKNOWN) {
    return this->file ? this->file : "";
  }
  if (this->host == nullptr && this->file == nullptr && label &&
      this->label != nullptr) {
    /* local label */
    return Sprintf("#%s", this->label)->ptr;
  }
  if (this->schema == SCM_LOCAL && !strcmp(this->file, "-")) {
    tmp = Strnew_charp("-");
    if (label && this->label) {
      Strcat_char(tmp, '#');
      Strcat_charp(tmp, this->label);
    }
    return tmp->ptr;
  }
  tmp = Strnew_charp(schema_str[this->schema]);
  Strcat_char(tmp, ':');
  if (this->schema == SCM_MAILTO) {
    Strcat_charp(tmp, this->file);
    if (this->query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, this->query);
    }
    return tmp->ptr;
  }
  if (this->schema == SCM_DATA) {
    Strcat_charp(tmp, this->file);
    return tmp->ptr;
  }
  { Strcat_charp(tmp, "//"); }
  if (user && this->user) {
    Strcat_charp(tmp, this->user);
    if (pass && this->pass) {
      Strcat_char(tmp, ':');
      Strcat_charp(tmp, this->pass);
    }
    Strcat_char(tmp, '@');
  }
  if (this->host) {
    Strcat_charp(tmp, this->host);
    if (this->port != getDefaultPort(this->schema)) {
      Strcat_char(tmp, ':');
      Strcat(tmp, Sprintf("%d", this->port));
    }
  }
  if ((this->file == nullptr ||
       (this->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        && !(IS_ALPHA(this->file[0]) && this->file[1] == ':' &&
             this->host == nullptr)
#endif
            )))
    Strcat_char(tmp, '/');
  Strcat_charp(tmp, this->file);
  if (this->schema == SCM_FTPDIR && Strlastchar(tmp) != '/')
    Strcat_char(tmp, '/');
  if (this->query) {
    Strcat_char(tmp, '?');
    Strcat_charp(tmp, this->query);
  }
  if (label && this->label) {
    Strcat_char(tmp, '#');
    Strcat_charp(tmp, this->label);
  }
  return tmp->ptr;
}

bool Url::same_url_p(const Url *pu2) const {
  return (
      this->schema == pu2->schema && this->port == pu2->port &&
      (this->host ? pu2->host ? !strcasecmp(this->host, pu2->host) : 0 : 1) &&
      (this->file ? pu2->file ? !strcmp(this->file, pu2->file) : 0 : 1));
}

static auto xdigit = "0123456789ABCDEF";

char url_unquote_char(const char **pstr) {
  return ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))
              ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) |
                                   GET_MYCDIGIT((*(pstr))[-1]))
              : -1);
}

const char *url_quote(const char *str) {
  Str *tmp = NULL;
  for (auto p = str; *p; p++) {
    if (is_url_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '%');
      Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
      Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

Str *Str_url_unquote(Str *x, int is_form, int safe) {
  Str *tmp = NULL;
  const char *p = x->ptr, *ep = x->ptr + x->length, *q;
  int c;

  for (; p < ep;) {
    if (is_form && *p == '+') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, ' ');
      p++;
      continue;
    } else if (*p == '%') {
      q = p;
      c = url_unquote_char(&q);
      if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
        if (tmp == NULL)
          tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
        Strcat_char(tmp, (char)c);
        p = q;
        continue;
      }
    }
    if (tmp)
      Strcat_char(tmp, *p);
    p++;
  }
  if (tmp)
    return tmp;
  return x;
}

const char *url_unquote_conv0(const char *url) {
  Str *tmp;
  tmp = Str_url_unquote(Strnew_charp(url), false, true);
  return tmp->ptr;
}

const char *url_decode0(const char *url) {
  if (!DecodeURL)
    return url;
  return url_unquote_conv(url, 0);
}

const char *file_quote(const char *str) {
  Str *tmp = NULL;
  const char *p;
  char buf[4];

  for (p = str; *p; p++) {
    if (is_file_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      sprintf(buf, "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

const char *file_unquote(const char *str) {
  Str *tmp = NULL;
  const char *p, *q;
  int c;

  for (p = str; *p;) {
    if (*p == '%') {
      q = p;
      c = url_unquote_char(&q);
      if (c >= 0) {
        if (tmp == NULL)
          tmp = Strnew_charp_n(str, (int)(p - str));
        if (c != '\0' && c != '\n' && c != '\r')
          Strcat_char(tmp, (char)c);
        p = q;
        continue;
      }
    }
    if (tmp)
      Strcat_char(tmp, *p);
    p++;
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

const char *cleanupName(const char *name) {
  auto buf = allocStr(name, -1);
  auto p = buf;
  auto q = name;
  while (*q != '\0') {
    if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../../       */
        p += 3;
        q += 3;
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../../    */
        p += 3;
        q += 3;
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/FOO */
        *p = '\0';
        q += 3;
        strcat(buf, q);
      }
    } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
      if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
        /* ../..        */
      } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
        /* ../../..     */
      } else {
        while (p != buf && *--p != '/')
          ; /* ->foo/ */
        *++p = '\0';
      }
      break;
    } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
      *p = '\0';                            /* -> foo/bar           */
      q += 2;
      strcat(buf, q);
    } else if (strcmp(p, "/.") == 0) { /* foo/. */
      *++p = '\0';                     /* -> foo/              */
      break;
    } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
      /* -> foo/bar           */
      *p = '\0';
      q++;
      strcat(buf, q);
    } else {
      p++;
      q++;
    }
  }
  return buf;
}
