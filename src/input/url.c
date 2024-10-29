#include "input/url.h"
#include "alloc.h"
#include "core.h"
#include "os.h"
#include "text/myctype.h"
#include "text/text.h"
#include <string.h>

bool DecodeURL = false;
const char *HostName = nullptr;

struct {
  const char *cmdname;
  enum URL_SCHEME_TYPE cmd;
} schemetable[] = {
    {"http", SCM_HTTP},
    {"gopher", SCM_GOPHER},
    {"ftp", SCM_FTP},
    {"local", SCM_LOCAL},
    {"file", SCM_LOCAL},
    /*  {"exec", SCM_EXEC}, */
    {"nntp", SCM_NNTP},
    /*  {"nntp", SCM_NNTP_GROUP}, */
    {"news", SCM_NEWS},
    /*  {"news", SCM_NEWS_GROUP}, */
    {"data", SCM_DATA},
    {"mailto", SCM_MAILTO},
    {"https", SCM_HTTPS},
    {NULL, SCM_UNKNOWN},
};

static const char *scheme_str[] = {
    "http", "gopher", "ftp",  "ftp",  "file", "file",   "exec",
    "nntp", "nntp",   "news", "news", "data", "mailto", "https",
};

int DefaultPort[] = {
    80,  /* http */
    70,  /* gopher */
    21,  /* ftp */
    21,  /* ftpdir */
    0,   /* local - not defined */
    0,   /* local-CGI - not defined? */
    0,   /* exec - not defined? */
    119, /* nntp */
    119, /* nntp group */
    119, /* news */
    119, /* news group */
    0,   /* data - not defined */
    0,   /* mailto - not defined */
    443, /* https */
};

const char *schemeNumToName(enum URL_SCHEME_TYPE scheme) {
  for (int i = 0; schemetable[i].cmdname != NULL; i++) {
    if (schemetable[i].cmd == scheme)
      return schemetable[i].cmdname;
  }
  return NULL;
}

enum URL_SCHEME_TYPE getURLScheme(const char **url) {
  const char *p = *url, *q;
  int i;
  int scheme = SCM_MISSING;

  while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
    p++;
  if (*p == ':') { /* scheme found */
    scheme = SCM_UNKNOWN;
    for (i = 0; (q = schemetable[i].cmdname) != NULL; i++) {
      int len = strlen(q);
      if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
        scheme = schemetable[i].cmd;
        *url = p + 1;
        break;
      }
    }
  }
  return scheme;
}

int is_localhost(const char *host) {
  if (!host || !strcasecmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
      (HostName && !strcasecmp(host, HostName)) || !strcmp(host, "[::1]"))
    return true;
  return false;
}

const char *file_to_url(const char *file) {
  char *drive = NULL;
  file = expandPath(file);
  if (IS_ALPHA(file[0]) && file[1] == ':') {
    drive = allocStr(file, 2);
    file += 2;
  } else if (file[0] != '/') {
    auto tmp = Strnew_charp(getCurrentDir());
    if (Strlastchar(tmp) != '/')
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, file);
    file = tmp->ptr;
  }
  auto tmp = Strnew_charp("file://");
  if (drive) {
    Strcat_charp(tmp, drive);
  }
  Strcat_charp(tmp, file_quote(cleanupName(file)));
  return tmp->ptr;
}

const char *url_unquote_conv0(const char *url) {
  return Str_url_unquote(Strnew_charp(url), false, true)->ptr;
}

void parseURL2(const char *url, struct Url *pu, struct Url *current) {
  const char *p;
  Str tmp;
  int relative_uri = false;

  parseURL(url, pu, current);
  if (pu->scheme == SCM_MAILTO)
    return;
  if (pu->scheme == SCM_DATA)
    return;
  if (pu->scheme == SCM_NEWS || pu->scheme == SCM_NEWS_GROUP) {
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->scheme = SCM_NEWS_GROUP;
    else
      pu->scheme = SCM_NEWS;
    return;
  }
  if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NNTP_GROUP) {
    if (pu->file && *pu->file == '/')
      pu->file = allocStr(pu->file + 1, -1);
    if (pu->file && !strchr(pu->file, '@') &&
        (!(p = strchr(pu->file, '/')) || strchr(p + 1, '-') ||
         *(p + 1) == '\0'))
      pu->scheme = SCM_NNTP_GROUP;
    else
      pu->scheme = SCM_NNTP;
    if (current &&
        (current->scheme == SCM_NNTP || current->scheme == SCM_NNTP_GROUP)) {
      if (pu->host == NULL) {
        pu->host = current->host;
        pu->port = current->port;
      }
    }
    return;
  }
  if (pu->scheme == SCM_LOCAL) {
    auto q = expandName(file_unquote(pu->file));
    Str drive;
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      drive = Strnew_charp_n(q, 2);
      Strcat_charp(drive, file_quote(q + 2));
      pu->file = drive->ptr;
    } else
      pu->file = file_quote(q);
  }

  if (current &&
      (pu->scheme == current->scheme ||
       (pu->scheme == SCM_FTP && current->scheme == SCM_FTPDIR) ||
       (pu->scheme == SCM_LOCAL && current->scheme == SCM_LOCAL_CGI)) &&
      pu->host == NULL) {
    /* Copy omitted element from the current URL */
    pu->user = current->user;
    pu->pass = current->pass;
    pu->host = current->host;
    pu->port = current->port;
    if (pu->file && *pu->file) {
      if (pu->file[0] != '/' &&
          !(pu->scheme == SCM_LOCAL && IS_ALPHA(pu->file[0]) &&
            pu->file[1] == ':')) {
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
    } else { /* scheme:[?query][#label] */
      pu->file = current->file;
      if (!pu->query)
        pu->query = current->query;
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }
  if (pu->file) {
    if (pu->scheme == SCM_LOCAL && pu->file[0] != '/' &&
        !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':') &&
        strcmp(pu->file, "-")) {
      /* local file, relative path */
      tmp = Strnew_charp(getCurrentDir());
      if (Strlastchar(tmp) != '/')
        Strcat_char(tmp, '/');
      Strcat_charp(tmp, file_unquote(pu->file));
      pu->file = file_quote(cleanupName(tmp->ptr));
    } else if (pu->scheme == SCM_HTTP || pu->scheme == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, pu->file is created by [process 1] above.
         * pu->file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the pu->file. It is
         * server's responsibility to canonicalize such path.
         */
        pu->file = cleanupName(pu->file);
      }
    } else if (pu->file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp scheme (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      pu->file = cleanupName(pu->file);
    }
    if (pu->scheme == SCM_LOCAL) {
#ifdef SUPPORT_NETBIOS_SHARE
      if (pu->host && !is_localhost(pu->host)) {
        Str tmp = Strnew_charp("//");
        Strcat_m_charp(tmp, pu->host, cleanupName(file_unquote(pu->file)),
                       NULL);
        pu->real_file = tmp->ptr;
      } else
#endif
        pu->real_file = cleanupName(file_unquote(pu->file));
    }
  }
}

const char *url_decode0(const char *url) {
  if (!DecodeURL)
    return url;
  return url_unquote_conv0(url);
}

static char xdigit[0x10] = "0123456789ABCDEF";

const char *url_quote(const char *str) {
  Str tmp = NULL;
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

#define url_unquote_char(pstr)                                                 \
  ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))                        \
       ? (*(pstr) += 3,                                                        \
          (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1]))    \
       : -1)

const char *file_unquote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p;) {
    if (*p == '%') {
      auto q = p;
      int c = url_unquote_char(&q);
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

Str Str_url_unquote(Str x, bool is_form, bool safe) {
  Str tmp = NULL;
  char *p = x->ptr, *ep = x->ptr + x->length, *q;
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

const char *file_quote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p; p++) {
    if (is_file_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      char buf[4];
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

enum CopyPathOption {
  COPYPATH_SPC_ALLOW = 0,
  COPYPATH_SPC_IGNORE = 1,
  COPYPATH_SPC_REPLACE = 2,
  COPYPATH_SPC_MASK = 3,
  COPYPATH_LOWERCASE = 4,
};

static char *copyPath(const char *orgpath, int length,
                      enum CopyPathOption option) {
  Str tmp = Strnew();
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

/* #define HTTP_DEFAULT_FILE    "/index.html" */
#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

static char *DefaultFile(int scheme) {
  switch (scheme) {
  case SCM_HTTP:
  case SCM_HTTPS:
    return allocStr(HTTP_DEFAULT_FILE, -1);
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
  case SCM_FTP:
  case SCM_FTPDIR:
    return allocStr("/", -1);
  }
  return NULL;
}

void parseURL(const char *_url, struct Url *p_url, struct Url *current) {
  const char *q, *qq;
  Str tmp;

  auto url = url_quote(_url); /* quote 0x01-0x20, 0x7F-0xFF */

  auto p = url;
  copyParsedURL(p_url, NULL);
  p_url->scheme = SCM_MISSING;

  /* RFC1808: Relative Uniform Resource Locators
   * 4.  Resolving Relative URLs
   */
  if (*url == '\0' || *url == '#') {
    if (current)
      copyParsedURL(p_url, current);
    goto do_label;
  }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (IS_ALPHA(*p) && (p[1] == ':' || p[1] == '|')) {
    p_url->scheme = SCM_LOCAL;
    goto analyze_file;
  }
#endif /* SUPPORT_DOS_DRIVE_PREFIX */
  /* search for scheme */
  p_url->scheme = getURLScheme(&p);
  if (p_url->scheme == SCM_MISSING) {
    /* scheme part is not found in the url. This means either
     * (a) the url is relative to the current or (b) the url
     * denotes a filename (therefore the scheme is SCM_LOCAL).
     */
    if (current) {
      switch (current->scheme) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        p_url->scheme = SCM_LOCAL;
        break;
      case SCM_FTP:
      case SCM_FTPDIR:
        p_url->scheme = SCM_FTP;
        break;
      default:
        p_url->scheme = current->scheme;
        break;
      }
    } else
      p_url->scheme = SCM_LOCAL;
    p = url;
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
  if (p_url->scheme == SCM_UNKNOWN) {
    p_url->file = allocStr(url, -1);
    return;
  }
  /* get host and port */
  if (p[0] != '/' || p[1] != '/') { /* scheme:foo or scheme:/foo */
    p_url->host = NULL;
    if (p_url->scheme != SCM_UNKNOWN)
      p_url->port = DefaultPort[p_url->scheme];
    else
      p_url->port = 0;
    goto analyze_file;
  }
  /* after here, p begins with // */
  if (p_url->scheme == SCM_LOCAL) { /* file://foo           */
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
  if (*q == '[') { /* rfc2732,rfc2373 compliance */
    p++;
    while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
      p++;
    if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
      p = q;
  }
  while (*p && strchr(":/@?#", *p) == NULL)
    p++;
  switch (*p) {
  case ':':
    /* scheme://user:pass@host or
     * scheme://host:port
     */
    qq = q;
    q = ++p;
    while (*p && strchr("@/?#", *p) == NULL)
      p++;
    if (*p == '@') {
      /* scheme://user:pass@...       */
      p_url->user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
      p_url->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
      p++;
      goto analyze_url;
    }
    /* scheme://host:port/ */
    p_url->host =
        copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    tmp = Strnew_charp_n(q, p - q);
    p_url->port = atoi(tmp->ptr);
    /* *p is one of ['\0', '/', '?', '#'] */
    break;
  case '@':
    /* scheme://user@...            */
    p_url->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    p++;
    goto analyze_url;
  case '\0':
    /* scheme://host                */
  case '/':
  case '?':
  case '#':
    p_url->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    if (p_url->scheme != SCM_UNKNOWN)
      p_url->port = DefaultPort[p_url->scheme];
    else
      p_url->port = 0;
    break;
  }
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
  if (p_url->scheme == SCM_LOCAL && p_url->user == NULL &&
      p_url->host != NULL && *p_url->host != '\0' &&
      !is_localhost(p_url->host)) {
    /*
     * In the environments other than CYGWIN, a URL like
     * file://host/file is regarded as ftp://host/file.
     * On the other hand, file://host/file on CYGWIN is
     * regarded as local access to the file //host/file.
     * `host' is a netbios-hostname, drive, or any other
     * name; It is CYGWIN system call who interprets that.
     */

    p_url->scheme = SCM_FTP; /* ftp://host/... */
    if (p_url->port == 0)
      p_url->port = DefaultPort[SCM_FTP];
  }
#endif
  if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == NULL) {
    p_url->file = "";
    goto do_query;
  }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (p_url->scheme == SCM_LOCAL) {
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
  if (*p == '\0' || *p == '#' || *p == '?') { /* scheme://host[:port]/ */
    p_url->file = DefaultFile(p_url->scheme);
    goto do_query;
  }
  {
    char *cgi = strchr(p, '?');
  again:
    while (*p && *p != '#' && p != cgi)
      p++;
    if (*p == '#' && p_url->scheme == SCM_LOCAL) {
      /*
       * According to RFC2396, # means the beginning of
       * URI-reference, and # should be escaped.  But,
       * if the scheme is SCM_LOCAL, the special
       * treatment will apply to # for convinience.
       */
      if (p > q && *(p - 1) == '/' && (cgi == NULL || p < cgi)) {
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
    if (p_url->scheme == SCM_LOCAL || p_url->scheme == SCM_MISSING)
      p_url->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    else
      p_url->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    p_url->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }
do_label:
  if (p_url->scheme == SCM_MISSING) {
    p_url->scheme = SCM_LOCAL;
    p_url->file = allocStr(p, -1);
    p_url->label = NULL;
  } else if (*p == '#')
    p_url->label = allocStr(p + 1, -1);
  else
    p_url->label = NULL;
}

Str _parsedURL2Str(struct Url *pu, int pass, int user, int label) {
  if (pu->scheme == SCM_MISSING) {
    return Strnew_charp("???");
  } else if (pu->scheme == SCM_UNKNOWN) {
    return Strnew_charp(pu->file);
  }
  if (pu->host == NULL && pu->file == NULL && label && pu->label != NULL) {
    /* local label */
    return Sprintf("#%s", pu->label);
  }

  if (pu->scheme == SCM_LOCAL && !strcmp(pu->file, "-")) {
    auto tmp = Strnew_charp("-");
    if (label && pu->label) {
      Strcat_char(tmp, '#');
      Strcat_charp(tmp, pu->label);
    }
    return tmp;
  }

  auto tmp = Strnew_charp(scheme_str[pu->scheme]);
  Strcat_char(tmp, ':');
  if (pu->scheme == SCM_MAILTO) {
    Strcat_charp(tmp, pu->file);
    if (pu->query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, pu->query);
    }
    return tmp;
  }
  if (pu->scheme == SCM_DATA) {
    Strcat_charp(tmp, pu->file);
    return tmp;
  }
  {
    Strcat_charp(tmp, "//");
  }
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
    if (pu->port != DefaultPort[pu->scheme]) {
      Strcat_char(tmp, ':');
      Strcat(tmp, Sprintf("%d", pu->port));
    }
  }
  if ((pu->file == NULL ||
       (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == NULL)
#endif
            )))
    Strcat_char(tmp, '/');
  Strcat_charp(tmp, pu->file);
  if (pu->scheme == SCM_FTPDIR && Strlastchar(tmp) != '/')
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
