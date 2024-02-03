#include "w3m.h"
#include "etc.h"
#include "message.h"
#include "ssl_util.h"
#include "screen.h"
#include "display.h"
#include "terms.h"
#include "indep.h"
#include "httprequest.h"
#include "rc.h"
#include "file.h"
#include "cookie.h"
#include "buffer.h"
#include "indep.h"
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
  if (buf->baseURL != nullptr) {
    /* <BASE> tag is defined in the document */
    return buf->baseURL;
  } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
    return nullptr;
  else
    return &buf->currentURL;
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

void parseURL2(const char *url, Url *pu, const Url *current) {
  const char *p;
  Str *tmp;
  int relative_uri = false;

  *pu = Url::parse(url, current);
  if (pu->schema == SCM_MAILTO)
    return;
  if (pu->schema == SCM_DATA)
    return;
  if (pu->schema == SCM_NEWS || pu->schema == SCM_NEWS_GROUP) {
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->schema = SCM_NEWS_GROUP;
    else
      pu->schema = SCM_NEWS;
    return;
  }
  if (pu->schema == SCM_NNTP || pu->schema == SCM_NNTP_GROUP) {
    if (pu->file && *pu->file == '/')
      pu->file = allocStr(pu->file + 1, -1);
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->schema = SCM_NNTP_GROUP;
    else
      pu->schema = SCM_NNTP;
    if (current &&
        (current->schema == SCM_NNTP || current->schema == SCM_NNTP_GROUP)) {
      if (pu->host == nullptr) {
        pu->host = current->host;
        pu->port = current->port;
      }
    }
    return;
  }
  if (pu->schema == SCM_LOCAL) {
    auto q = expandName(file_unquote((char *)pu->file));
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    Str *drive;
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      drive = Strnew_charp_n(q, 2);
      Strcat_charp(drive, file_quote(q + 2));
      pu->file = drive->ptr;
    } else
#endif
      pu->file = file_quote((char *)q);
  }

  if (current &&
      (pu->schema == current->schema ||
       (pu->schema == SCM_FTP && current->schema == SCM_FTPDIR) ||
       (pu->schema == SCM_LOCAL && current->schema == SCM_LOCAL_CGI)) &&
      pu->host == nullptr) {
    /* Copy omitted element from the current URL */
    pu->user = current->user;
    pu->pass = current->pass;
    pu->host = current->host;
    pu->port = current->port;
    if (pu->file && *pu->file) {
      if (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
          && !(pu->schema == SCM_LOCAL && IS_ALPHA(pu->file[0]) &&
               pu->file[1] == ':')
#endif
      ) {
        /* file is relative [process 1] */
        p = pu->file;
        if (current->file) {
          tmp = Strnew_charp(current->file);
          while (tmp->length > 0) {
            if (Strlastchar(tmp) == '/')
              break;
            Strshrink(tmp, 1);
          }
          Strcat_charp(tmp, p);
          pu->file = tmp->ptr;
          relative_uri = true;
        }
      }
    } else { /* schema:[?query][#label] */
      pu->file = current->file;
      if (!pu->query)
        pu->query = current->query;
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }
  if (pu->file) {
    if (pu->schema == SCM_LOCAL && pu->file[0] != '/' &&
#ifdef SUPPORT_DOS_DRIVE_PREFIX /* for 'drive:' */
        !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
#endif
        strcmp(pu->file, "-")) {
      /* local file, relative path */
      tmp = Strnew_charp(CurrentDir);
      if (Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
      Strcat_charp(tmp, file_unquote((char *)pu->file));
      pu->file = file_quote(cleanupName(tmp->ptr));
    } else if (pu->schema == SCM_HTTP || pu->schema == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, pu->file is created by [process 1] above.
         * pu->file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the pu->file. It is
         * server's responsibility to canonicalize such path.
         */
        pu->file = cleanupName((char *)pu->file);
      }
    } else if (pu->file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp schema (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      pu->file = cleanupName((char *)pu->file);
    }
    if (pu->schema == SCM_LOCAL) {
#ifdef SUPPORT_NETBIOS_SHARE
      if (pu->host && !is_localhost(pu->host)) {
        Str *tmp = Strnew_charp("//");
        Strcat_m_charp(tmp, pu->host, cleanupName(file_unquote(pu->file)),
                       nullptr);
        pu->real_file = tmp->ptr;
      } else
#endif
        pu->real_file = cleanupName(file_unquote(pu->file));
    }
  }
}

Str *_Url2Str(const Url *pu, int pass, int user, int label) {
  Str *tmp;
  static const char *schema_str[] = {
      "http", "gopher", "ftp",  "ftp",  "file", "file",   "exec",
      "nntp", "nntp",   "news", "news", "data", "mailto", "https",
  };

  if (pu->schema == SCM_MISSING) {
    return Strnew_charp("???");
  } else if (pu->schema == SCM_UNKNOWN) {
    return Strnew_charp(pu->file);
  }
  if (pu->host == nullptr && pu->file == nullptr && label &&
      pu->label != nullptr) {
    /* local label */
    return Sprintf("#%s", pu->label);
  }
  if (pu->schema == SCM_LOCAL && !strcmp(pu->file, "-")) {
    tmp = Strnew_charp("-");
    if (label && pu->label) {
      Strcat_char(tmp, '#');
      Strcat_charp(tmp, pu->label);
    }
    return tmp;
  }
  tmp = Strnew_charp(schema_str[pu->schema]);
  Strcat_char(tmp, ':');
  if (pu->schema == SCM_MAILTO) {
    Strcat_charp(tmp, pu->file);
    if (pu->query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, pu->query);
    }
    return tmp;
  }
  if (pu->schema == SCM_DATA) {
    Strcat_charp(tmp, pu->file);
    return tmp;
  }
  { Strcat_charp(tmp, "//"); }
  if (user && pu->user) {
    Strcat_charp(tmp, pu->user);
    if (pass && pu->pass) {
      Strcat_char(tmp, ':');
      Strcat_charp(tmp, pu->pass);
    }
    Strcat_char(tmp, '@');
  }
  if (pu->host) {
    Strcat_charp(tmp, pu->host);
    if (pu->port != getDefaultPort(pu->schema)) {
      Strcat_char(tmp, ':');
      Strcat(tmp, Sprintf("%d", pu->port));
    }
  }
  if ((pu->file == nullptr ||
       (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == nullptr)
#endif
            )))
    Strcat_char(tmp, '/');
  Strcat_charp(tmp, pu->file);
  if (pu->schema == SCM_FTPDIR && Strlastchar(tmp) != '/')
    Strcat_char(tmp, '/');
  if (pu->query) {
    Strcat_char(tmp, '?');
    Strcat_charp(tmp, pu->query);
  }
  if (label && pu->label) {
    Strcat_char(tmp, '#');
    Strcat_charp(tmp, pu->label);
  }
  return tmp;
}

Str *Url2Str(const Url *pu) { return _Url2Str(pu, false, true, true); }

Str *Url2RefererStr(const Url *pu) { return _Url2Str(pu, false, false, false); }

Url *schemaToProxy(UrlSchema schema) {
  Url *pu = nullptr; /* for gcc */
  switch (schema) {
  case SCM_HTTP:
    pu = &HTTP_proxy_parsed;
    break;
  case SCM_HTTPS:
    pu = &HTTPS_proxy_parsed;
    break;
  case SCM_FTP:
    pu = &FTP_proxy_parsed;
    break;
  default:
#ifdef DEBUG
    abort();
#endif
    break;
  }
  return pu;
}

const char *url_decode0(const char *url) {
  if (!DecodeURL)
    return url;
  return url_unquote_conv(url, 0);
}

bool Url::same_url_p(const Url *pu2) const {
  return (
      this->schema == pu2->schema && this->port == pu2->port &&
      (this->host ? pu2->host ? !strcasecmp(this->host, pu2->host) : 0 : 1) &&
      (this->file ? pu2->file ? !strcmp(this->file, pu2->file) : 0 : 1));
}

bool checkRedirection(const Url *pu) {
  static std::vector<Url> redirectins;

  if (pu == nullptr) {
    // clear
    redirectins.clear();
    return true;
  }

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, Url2Str(pu)->ptr);
    disp_err_message(tmp->ptr, false);
    return false;
  }

  for (auto &url : redirectins) {
    if (pu->same_url_p(&url)) {
      // same url found !
      auto tmp = Sprintf("Redirection loop detected (%s)", Url2Str(pu)->ptr);
      disp_err_message(tmp->ptr, false);
      return false;
    }
  }

  redirectins.push_back(*pu);

  // if (!puv) {
  //   nredir_size = FollowRedirection / 2 + 1;
  //   puv = (Url *)New_N(Url, nredir_size);
  //   memset(puv, 0, sizeof(Url) * nredir_size);
  // }
  // copyUrl(&puv[nredir % nredir_size], pu);
  // nredir++;
  return true;
}
