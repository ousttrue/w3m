#include "w3m.h"
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#include <sys/stat.h>

bool ArgvIsURL = true;
bool LocalhostOnly = false;
bool retryAsHttp = true;
char *index_file = nullptr;

char *HTTP_proxy = nullptr;
char *HTTPS_proxy = nullptr;
char *FTP_proxy = nullptr;
char *NO_proxy = nullptr;
int NOproxy_netaddr = true;
bool use_proxy = true;
const char *w3m_reqlog;
TextList *NO_proxy_domains;

int DNS_order = DNS_ORDER_UNSPEC;

ParsedURL HTTP_proxy_parsed;
ParsedURL HTTPS_proxy_parsed;
ParsedURL FTP_proxy_parsed;

#ifdef INET6
/* see rc.c, "dns_order" and dnsorders[] */
int ai_family_order_table[7][3] = {
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 0:unspec */
    {PF_INET, PF_INET6, PF_UNSPEC},    /* 1:inet inet6 */
    {PF_INET6, PF_INET, PF_UNSPEC},    /* 2:inet6 inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 3: --- */
    {PF_INET, PF_UNSPEC, PF_UNSPEC},   /* 4:inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 5: --- */
    {PF_INET6, PF_UNSPEC, PF_UNSPEC},  /* 6:inet6 */
};
#endif /* INET6 */

static JMP_BUF AbortLoading;

static void add_index_file(ParsedURL *pu, URLFile *uf);

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

#ifdef SOCK_DEBUG
#include <stdarg.h>

static void sock_log(char *message, ...) {
  FILE *f = fopen("zzzsocklog", "a");
  va_list va;

  if (f == NULL)
    return;
  va_start(va, message);
  vfprintf(f, message, va);
  fclose(f);
}

#endif

static char *DefaultFile(int schema) {
  switch (schema) {
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

static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
}

static void write_from_file(int sock, char *file) {
  FILE *fd;
  int c;
  char buf[1];
  fd = fopen(file, "r");
  if (fd != NULL) {
    while ((c = fgetc(fd)) != EOF) {
      buf[0] = c;
      write(sock, buf, 1);
    }
    fclose(fd);
  }
}

ParsedURL *baseURL(Buffer *buf) {
  if (buf->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return NULL;
  }
  if (buf->baseURL != NULL) {
    /* <BASE> tag is defined in the document */
    return buf->baseURL;
  } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
    return NULL;
  else
    return &buf->currentURL;
}

int openSocket(const char *const hostname, const char *remoteport_name,
               unsigned short remoteport_num) {
  volatile int sock = -1;
#ifdef INET6
  int *af;
  struct addrinfo hints, *res0, *res;
  int error;
  const char *hname;
#else  /* not INET6 */
  struct sockaddr_in hostaddr;
  struct hostent *entry;
  struct protoent *proto;
  unsigned short s_port;
  int a1, a2, a3, a4;
  unsigned long adr;
#endif /* not INET6 */
  MySignalHandler prevtrap = NULL;

  if (fmInitialized) {
    /* FIXME: gettextize? */
    message(Sprintf("Opening socket...")->ptr, 0, 0);
    refresh(term_io());
  }
  if (SETJMP(AbortLoading) != 0) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: user abort\n");
#endif
    if (sock >= 0)
      close(sock);
    goto error;
  }
  TRAP_ON;
  if (hostname == NULL) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n", hostname);
#endif
    goto error;
  }

#ifdef INET6
  /* rfc2732 compliance */
  hname = hostname;
  if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
    hname = allocStr(hostname + 1, -1);
    ((char *)hname)[strlen(hname) - 1] = '\0';
    if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
      goto error;
  }
  for (af = ai_family_order_table[DNS_order];; af++) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = *af;
    hints.ai_socktype = SOCK_STREAM;
    if (remoteport_num != 0) {
      Str *portbuf = Sprintf("%d", remoteport_num);
      error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
    } else {
      error = -1;
    }
    if (error && remoteport_name && remoteport_name[0] != '\0') {
      /* try default port */
      error = getaddrinfo(hname, remoteport_name, &hints, &res0);
    }
    if (error) {
      if (*af == PF_UNSPEC) {
        goto error;
      }
      /* try next ai family */
      continue;
    }
    sock = -1;
    for (res = res0; res; res = res->ai_next) {
      sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sock < 0) {
        continue;
      }
      if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        sock = -1;
        continue;
      }
      break;
    }
    if (sock < 0) {
      freeaddrinfo(res0);
      if (*af == PF_UNSPEC) {
        goto error;
      }
      /* try next ai family */
      continue;
    }
    freeaddrinfo(res0);
    break;
  }
#else /* not INET6 */
  s_port = htons(remoteport_num);
  bzero((char *)&hostaddr, sizeof(struct sockaddr_in));
  if ((proto = getprotobyname("tcp")) == NULL) {
    /* protocol number of TCP is 6 */
    proto = New(struct protoent);
    proto->p_proto = 6;
  }
  if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0) {
#ifdef SOCK_DEBUG
    sock_log("openSocket: socket() failed. reason: %s\n", strerror(errno));
#endif
    goto error;
  }
  regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
  if (regexMatch(hostname, -1, 1)) {
    sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
    adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
    bcopy((void *)&adr, (void *)&hostaddr.sin_addr, sizeof(long));
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = s_port;
    if (fmInitialized) {
      message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
      refresh();
    }
    if (connect(sock, (struct sockaddr *)&hostaddr,
                sizeof(struct sockaddr_in)) < 0) {
#ifdef SOCK_DEBUG
      sock_log("openSocket: connect() failed. reason: %s\n", strerror(errno));
#endif
      goto error;
    }
  } else {
    char **h_addr_list;
    int result = -1;
    if (fmInitialized) {
      message(Sprintf("Performing hostname lookup on %s", hostname)->ptr, 0, 0);
      refresh();
    }
    if ((entry = gethostbyname(hostname)) == NULL) {
#ifdef SOCK_DEBUG
      sock_log("openSocket: gethostbyname() failed. reason: %s\n",
               strerror(errno));
#endif
      goto error;
    }
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = s_port;
    for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++) {
      bcopy((void *)h_addr_list[0], (void *)&hostaddr.sin_addr,
            entry->h_length);
#ifdef SOCK_DEBUG
      adr = ntohl(*(long *)&hostaddr.sin_addr);
      sock_log("openSocket: connecting %d.%d.%d.%d\n", (adr >> 24) & 0xff,
               (adr >> 16) & 0xff, (adr >> 8) & 0xff, adr & 0xff);
#endif
      if (fmInitialized) {
        message(Sprintf("Connecting to %s", hostname)->ptr, 0, 0);
        refresh();
      }
      if ((result = connect(sock, (struct sockaddr *)&hostaddr,
                            sizeof(struct sockaddr_in))) == 0) {
        break;
      }
#ifdef SOCK_DEBUG
      else {
        sock_log("openSocket: connect() failed. reason: %s\n", strerror(errno));
      }
#endif
    }
    if (result < 0) {
      goto error;
    }
  }
#endif /* not INET6 */

  TRAP_OFF;
  return sock;
error:
  TRAP_OFF;
  return -1;
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

void parseURL(char *url, ParsedURL *p_url, ParsedURL *current) {
  const char *p, *q, *qq;
  Str *tmp;

  url = url_quote(url); /* quote 0x01-0x20, 0x7F-0xFF */

  p = url;
  copyParsedURL(p_url, NULL);
  p_url->schema = SCM_MISSING;

  /* RFC1808: Relative Uniform Resource Locators
   * 4.  Resolving Relative URLs
   */
  if (*url == '\0' || *url == '#') {
    if (current)
      copyParsedURL(p_url, current);
    goto do_label;
  }
  /* search for schema */
  p_url->schema = parseUrlSchema(&p);
  if (p_url->schema == SCM_MISSING) {
    /* schema part is not found in the url. This means either
     * (a) the url is relative to the current or (b) the url
     * denotes a filename (therefore the schema is SCM_LOCAL).
     */
    if (current) {
      switch (current->schema) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        p_url->schema = SCM_LOCAL;
        break;
      case SCM_FTP:
      case SCM_FTPDIR:
        p_url->schema = SCM_FTP;
        break;
      default:
        p_url->schema = current->schema;
        break;
      }
    } else
      p_url->schema = SCM_LOCAL;
    p = url;
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
  if (p_url->schema == SCM_UNKNOWN) {
    p_url->file = allocStr(url, -1);
    return;
  }
  /* get host and port */
  if (p[0] != '/' || p[1] != '/') { /* schema:foo or schema:/foo */
    p_url->host = NULL;
    p_url->port = getDefaultPort(p_url->schema);
    goto analyze_file;
  }
  /* after here, p begins with // */
  if (p_url->schema == SCM_LOCAL) { /* file://foo           */
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
    if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
      p = q;
  }
#endif
  while (*p && strchr(":/@?#", *p) == NULL)
    p++;
  switch (*p) {
  case ':':
    /* schema://user:pass@host or
     * schema://host:port
     */
    qq = q;
    q = ++p;
    while (*p && strchr("@/?#", *p) == NULL)
      p++;
    if (*p == '@') {
      /* schema://user:pass@...       */
      p_url->user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
      p_url->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
      p++;
      goto analyze_url;
    }
    /* schema://host:port/ */
    p_url->host =
        copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    tmp = Strnew_charp_n(q, p - q);
    p_url->port = atoi(tmp->ptr);
    /* *p is one of ['\0', '/', '?', '#'] */
    break;
  case '@':
    /* schema://user@...            */
    p_url->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    p++;
    goto analyze_url;
  case '\0':
    /* schema://host                */
  case '/':
  case '?':
  case '#':
    p_url->host = copyPath(q, p - q, COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
    p_url->port = getDefaultPort(p_url->schema);
    break;
  }
analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
  if (p_url->schema == SCM_LOCAL && p_url->user == NULL &&
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

    p_url->schema = SCM_FTP; /* ftp://host/... */
    if (p_url->port == 0)
      p_url->port = getDefaultPort(SCM_FTP);
  }
#endif
  if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == NULL) {
    p_url->file = "";
    goto do_query;
  }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
  if (p_url->schema == SCM_LOCAL) {
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
    p_url->file = DefaultFile(p_url->schema);
    goto do_query;
  }
  {
    auto cgi = strchr(p, '?');
  again:
    while (*p && *p != '#' && p != cgi)
      p++;
    if (*p == '#' && p_url->schema == SCM_LOCAL) {
      /*
       * According to RFC2396, # means the beginning of
       * URI-reference, and # should be escaped.  But,
       * if the schema is SCM_LOCAL, the special
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
    if (p_url->schema == SCM_LOCAL || p_url->schema == SCM_MISSING)
      p_url->file = (char *)copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    else
      p_url->file = (char *)copyPath(q, p - q, COPYPATH_SPC_IGNORE);
  }

do_query:
  if (*p == '?') {
    q = ++p;
    while (*p && *p != '#')
      p++;
    p_url->query = (char *)copyPath(q, p - q, COPYPATH_SPC_ALLOW);
  }
do_label:
  if (p_url->schema == SCM_MISSING) {
    p_url->schema = SCM_LOCAL;
    p_url->file = allocStr(p, -1);
    p_url->label = NULL;
  } else if (*p == '#')
    p_url->label = allocStr(p + 1, -1);
  else
    p_url->label = NULL;
}

#define ALLOC_STR(s) ((s) == NULL ? NULL : allocStr(s, -1))

void copyParsedURL(ParsedURL *p, const ParsedURL *q) {
  if (q == NULL) {
    memset(p, 0, sizeof(ParsedURL));
    p->schema = SCM_UNKNOWN;
    return;
  }
  p->schema = q->schema;
  p->port = q->port;
  p->is_nocache = q->is_nocache;
  p->user = ALLOC_STR(q->user);
  p->pass = ALLOC_STR(q->pass);
  p->host = ALLOC_STR(q->host);
  p->file = ALLOC_STR(q->file);
  p->real_file = ALLOC_STR(q->real_file);
  p->label = ALLOC_STR(q->label);
  p->query = ALLOC_STR(q->query);
}

void parseURL2(char *url, ParsedURL *pu, ParsedURL *current) {
  char *p;
  Str *tmp;
  int relative_uri = FALSE;

  parseURL(url, pu, current);
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
      if (pu->host == NULL) {
        pu->host = current->host;
        pu->port = current->port;
      }
    }
    return;
  }
  if (pu->schema == SCM_LOCAL) {
    char *q = expandName(file_unquote(pu->file));
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    Str *drive;
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      drive = Strnew_charp_n(q, 2);
      Strcat_charp(drive, file_quote(q + 2));
      pu->file = drive->ptr;
    } else
#endif
      pu->file = file_quote(q);
  }

  if (current &&
      (pu->schema == current->schema ||
       (pu->schema == SCM_FTP && current->schema == SCM_FTPDIR) ||
       (pu->schema == SCM_LOCAL && current->schema == SCM_LOCAL_CGI)) &&
      pu->host == NULL) {
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
          relative_uri = TRUE;
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
      Strcat_charp(tmp, file_unquote(pu->file));
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
        pu->file = cleanupName(pu->file);
      }
    } else if (pu->file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp schema (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      pu->file = cleanupName(pu->file);
    }
    if (pu->schema == SCM_LOCAL) {
#ifdef SUPPORT_NETBIOS_SHARE
      if (pu->host && !is_localhost(pu->host)) {
        Str *tmp = Strnew_charp("//");
        Strcat_m_charp(tmp, pu->host, cleanupName(file_unquote(pu->file)),
                       NULL);
        pu->real_file = tmp->ptr;
      } else
#endif
        pu->real_file = cleanupName(file_unquote(pu->file));
    }
  }
}

Str *_parsedURL2Str(ParsedURL *pu, int pass, int user, int label) {
  Str *tmp;
  static char *schema_str[] = {
      "http", "gopher", "ftp",  "ftp",  "file", "file",   "exec",
      "nntp", "nntp",   "news", "news", "data", "mailto", "https",
  };

  if (pu->schema == SCM_MISSING) {
    return Strnew_charp("???");
  } else if (pu->schema == SCM_UNKNOWN) {
    return Strnew_charp(pu->file);
  }
  if (pu->host == NULL && pu->file == NULL && label && pu->label != NULL) {
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
  if ((pu->file == NULL ||
       (pu->file[0] != '/'
#ifdef SUPPORT_DOS_DRIVE_PREFIX
        && !(IS_ALPHA(pu->file[0]) && pu->file[1] == ':' && pu->host == NULL)
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

Str *parsedURL2Str(ParsedURL *pu) {
  return _parsedURL2Str(pu, FALSE, TRUE, TRUE);
}

Str *parsedURL2RefererStr(ParsedURL *pu) {
  return _parsedURL2Str(pu, FALSE, FALSE, FALSE);
}



void init_stream(URLFile *uf, UrlSchema schema, input_stream *stream) {
  memset(uf, 0, sizeof(URLFile));
  uf->stream = stream;
  uf->schema = schema;
  uf->encoding = ENC_7BIT;
  uf->is_cgi = FALSE;
  uf->compression = CMP_NOCOMPRESS;
  uf->content_encoding = CMP_NOCOMPRESS;
  uf->guess_type = NULL;
  uf->ext = NULL;
  uf->modtime = -1;
}

URLFile openURL(char *url, ParsedURL *pu, ParsedURL *current, URLOption *option,
                FormList *request, TextList *extra_header, URLFile *ouf,
                HRequest *hr, unsigned char *status) {
  Str *tmp;
  int sock;
  const char *p, *q, *u;
  URLFile uf;
  HRequest hr0;
  SSL *sslh = NULL;

  if (hr == NULL)
    hr = &hr0;

  if (ouf) {
    uf = *ouf;
  } else {
    init_stream(&uf, SCM_MISSING, NULL);
  }

  u = url;
  auto schema = parseUrlSchema(&u);
  if (current == NULL && schema == SCM_MISSING && !ArgvIsURL)
    u = file_to_url(url); /* force to local file */
  else
    u = url;
retry:
  parseURL2((char *)u, pu, current);
  if (pu->schema == SCM_LOCAL && pu->file == NULL) {
    if (pu->label != NULL) {
      /* #hogege is not a label but a filename */
      Str *tmp2 = Strnew_charp("#");
      Strcat_charp(tmp2, pu->label);
      pu->file = tmp2->ptr;
      pu->real_file = cleanupName(file_unquote(pu->file));
      pu->label = NULL;
    } else {
      /* given URL must be null string */
#ifdef SOCK_DEBUG
      sock_log("given URL must be null string\n");
#endif
      return uf;
    }
  }

  if (LocalhostOnly && pu->host && !is_localhost(pu->host))
    pu->host = NULL;

  uf.schema = pu->schema;
  uf.url = parsedURL2Str(pu)->ptr;
  pu->is_nocache = (option->flag & RG_NOCACHE);
  uf.ext = filename_extension(pu->file, 1);

  hr->command = HR_COMMAND_GET;
  hr->flag = 0;
  hr->referer = option->referer;
  hr->request = request;

  switch (pu->schema) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    if (request && request->body)
      /* local CGI: POST */
      uf.stream = newFileStream(
          localcgi_post(pu->real_file, pu->query, request, option->referer),
          (void (*)())fclose);
    else
      /* lodal CGI: GET */
      uf.stream =
          newFileStream(localcgi_get(pu->real_file, pu->query, option->referer),
                        (void (*)())fclose);
    if (uf.stream) {
      uf.is_cgi = TRUE;
      uf.schema = pu->schema = SCM_LOCAL_CGI;
      return uf;
    }
    examineFile(pu->real_file, &uf);
    if (uf.stream == NULL) {
      if (dir_exist(pu->real_file)) {
        add_index_file(pu, &uf);
        if (uf.stream == NULL)
          return uf;
      } else if (document_root != NULL) {
        tmp = Strnew_charp(document_root);
        if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
          Strcat_char(tmp, '/');
        Strcat_charp(tmp, pu->file);
        p = cleanupName(tmp->ptr);
        q = cleanupName(file_unquote((char *)p));
        if (dir_exist((char *)q)) {
          pu->file = (char *)p;
          pu->real_file = (char *)q;
          add_index_file(pu, &uf);
          if (uf.stream == NULL) {
            return uf;
          }
        } else {
          examineFile((char *)q, &uf);
          if (uf.stream) {
            pu->file = (char *)p;
            pu->real_file = (char *)q;
          }
        }
      }
    }
    if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
      if (schema == SCM_MISSING || schema == SCM_UNKNOWN) {
        /* retry it as "http://" */
        u = Strnew_m_charp("http://", url, NULL)->ptr;
        goto retry;
      }
    }
    return uf;
  case SCM_FTP:
  case SCM_FTPDIR:
    // if (pu->file == NULL)
    //   pu->file = allocStr("/", -1);
    // if (non_null(FTP_proxy) && !Do_not_use_proxy && pu->host != NULL &&
    //     !check_no_proxy(pu->host)) {
    //   hr->flag |= HR_FLAG_PROXY;
    //   sock = openSocket(FTP_proxy_parsed.host,
    //                     schemaNumToName(FTP_proxy_parsed.schema),
    //                     FTP_proxy_parsed.port);
    //   if (sock < 0)
    //     return uf;
    //   uf.schema = SCM_HTTP;
    //   tmp = HTTPrequest(pu, current, hr, extra_header);
    //   write(sock, tmp->ptr, tmp->length);
    // } else {
    //   uf.stream = openFTPStream(pu, &uf);
    //   uf.schema = pu->schema;
    //   return uf;
    // }
    break;
  case SCM_HTTP:
  case SCM_HTTPS:
    if (pu->file == NULL)
      pu->file = allocStr("/", -1);
    if (request && request->method == FORM_METHOD_POST && request->body)
      hr->command = HR_COMMAND_POST;
    if (request && request->method == FORM_METHOD_HEAD)
      hr->command = HR_COMMAND_HEAD;
    if (((pu->schema == SCM_HTTPS) ? non_null(HTTPS_proxy)
                                   : non_null(HTTP_proxy)) &&
        use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
      hr->flag |= HR_FLAG_PROXY;
      if (pu->schema == SCM_HTTPS && *status == HTST_CONNECT) {
        sock = ssl_socket_of(ouf->stream);
        if (!(sslh = openSSLHandle(sock, pu->host, &uf.ssl_certificate))) {
          *status = HTST_MISSING;
          return uf;
        }
      } else if (pu->schema == SCM_HTTPS) {
        sock = openSocket(HTTPS_proxy_parsed.host,
                          schemaNumToName(HTTPS_proxy_parsed.schema),
                          HTTPS_proxy_parsed.port);
        sslh = NULL;
      } else {
        sock = openSocket(HTTP_proxy_parsed.host,
                          schemaNumToName(HTTP_proxy_parsed.schema),
                          HTTP_proxy_parsed.port);
        sslh = NULL;
      }
      if (sock < 0) {
#ifdef SOCK_DEBUG
        sock_log("Can't open socket\n");
#endif
        return uf;
      }
      if (pu->schema == SCM_HTTPS) {
        if (*status == HTST_NORMAL) {
          hr->command = HR_COMMAND_CONNECT;
          tmp = HTTPrequest(pu, current, hr, extra_header);
          *status = HTST_CONNECT;
        } else {
          hr->flag |= HR_FLAG_LOCAL;
          tmp = HTTPrequest(pu, current, hr, extra_header);
          *status = HTST_NORMAL;
        }
      } else {
        tmp = HTTPrequest(pu, current, hr, extra_header);
        *status = HTST_NORMAL;
      }
    } else {
      sock = openSocket(pu->host, schemaNumToName(pu->schema), pu->port);
      if (sock < 0) {
        *status = HTST_MISSING;
        return uf;
      }
      if (pu->schema == SCM_HTTPS) {
        if (!(sslh = openSSLHandle(sock, pu->host, &uf.ssl_certificate))) {
          *status = HTST_MISSING;
          return uf;
        }
      }
      hr->flag |= HR_FLAG_LOCAL;
      tmp = HTTPrequest(pu, current, hr, extra_header);
      *status = HTST_NORMAL;
    }
    if (pu->schema == SCM_HTTPS) {
      uf.stream = newSSLStream(sslh, sock);
      if (sslh)
        SSL_write(sslh, tmp->ptr, tmp->length);
      else
        write(sock, tmp->ptr, tmp->length);
      if (w3m_reqlog) {
        FILE *ff = fopen(w3m_reqlog, "a");
        if (ff == NULL)
          return uf;
        if (sslh)
          fputs("HTTPS: request via SSL\n", ff);
        else
          fputs("HTTPS: request without SSL\n", ff);
        fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
        fclose(ff);
      }
      if (hr->command == HR_COMMAND_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        if (sslh)
          SSL_write_from_file(sslh, request->body);
        else
          write_from_file(sock, request->body);
      }
      return uf;
    } else {
      write(sock, tmp->ptr, tmp->length);
      if (w3m_reqlog) {
        FILE *ff = fopen(w3m_reqlog, "a");
        if (ff == NULL)
          return uf;
        fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
        fclose(ff);
      }
      if (hr->command == HR_COMMAND_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART)
        write_from_file(sock, request->body);
    }
    break;
  case SCM_DATA:
    if (pu->file == NULL)
      return uf;
    p = Strnew_charp(pu->file)->ptr;
    q = strchr(p, ',');
    if (q == NULL)
      return uf;
    *(char *)q++ = '\0';
    tmp = Strnew_charp(q);
    q = strrchr(p, ';');
    if (q != NULL && !strcmp(q, ";base64")) {
      *(char *)q = '\0';
      uf.encoding = ENC_BASE64;
    } else
      tmp = Str_url_unquote(tmp, FALSE, FALSE);
    uf.stream = newStrStream(tmp);
    uf.guess_type = (*p != '\0') ? p : "text/plain";
    return uf;
  case SCM_UNKNOWN:
  default:
    return uf;
  }
  uf.stream = newInputStream(sock);
  return uf;
}

/* add index_file if exists */
static void add_index_file(ParsedURL *pu, URLFile *uf) {
  char *p, *q;
  TextList *index_file_list = NULL;
  TextListItem *ti;

  if (non_null(index_file))
    index_file_list = make_domain_list(index_file);
  if (index_file_list == NULL) {
    uf->stream = NULL;
    return;
  }
  for (ti = index_file_list->first; ti; ti = ti->next) {
    p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
    p = cleanupName(p);
    q = cleanupName(file_unquote(p));
    examineFile(q, uf);
    if (uf->stream != NULL) {
      pu->file = p;
      pu->real_file = q;
      return;
    }
  }
}

static int domain_match(const char *pat, const char *domain) {
  if (domain == NULL)
    return 0;
  if (*pat == '.')
    pat++;
  for (;;) {
    if (!strcasecmp(pat, domain))
      return 1;
    domain = strchr(domain, '.');
    if (domain == NULL)
      return 0;
    domain++;
  }
}

int check_no_proxy(const char *domain) {
  TextListItem *tl;
  volatile int ret = 0;
  MySignalHandler prevtrap = NULL;

  if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 ||
      domain == NULL)
    return 0;
  for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
    if (domain_match(tl->ptr, domain))
      return 1;
  }
  if (!NOproxy_netaddr) {
    return 0;
  }
  /*
   * to check noproxy by network addr
   */
  if (SETJMP(AbortLoading) != 0) {
    ret = 0;
    goto end;
  }
  TRAP_ON;
  {
#ifndef INET6
    struct hostent *he;
    int n;
    unsigned char **h_addr_list;
    char addr[4 * 16], buf[5];

    he = gethostbyname(domain);
    if (!he) {
      ret = 0;
      goto end;
    }
    for (h_addr_list = (unsigned char **)he->h_addr_list; *h_addr_list;
         h_addr_list++) {
      sprintf(addr, "%d", h_addr_list[0][0]);
      for (n = 1; n < he->h_length; n++) {
        sprintf(buf, ".%d", h_addr_list[0][n]);
        strcat(addr, buf);
      }
      for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
          ret = 1;
          goto end;
        }
      }
    }
#else  /* INET6 */
    int error;
    struct addrinfo hints;
    struct addrinfo *res, *res0;
    char addr[4 * 16];
    int *af;

    for (af = ai_family_order_table[DNS_order];; af++) {
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = *af;
      error = getaddrinfo(domain, NULL, &hints, &res0);
      if (error) {
        if (*af == PF_UNSPEC) {
          break;
        }
        /* try next */
        continue;
      }
      for (res = res0; res != NULL; res = res->ai_next) {
        switch (res->ai_family) {
        case AF_INET:
          inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                    addr, sizeof(addr));
          break;
        case AF_INET6:
          inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
                    addr, sizeof(addr));
          break;
        default:
          /* unknown */
          continue;
        }
        for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
          if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
            freeaddrinfo(res0);
            ret = 1;
            goto end;
          }
        }
      }
      freeaddrinfo(res0);
      if (*af == PF_UNSPEC) {
        break;
      }
    }
#endif /* INET6 */
  }
end:
  TRAP_OFF;
  return ret;
}

char *filename_extension(char *path, int is_url) {
  char *last_dot = "", *p = path;
  int i;

  if (path == NULL)
    return last_dot;
  if (*p == '.')
    p++;
  for (; *p; p++) {
    if (*p == '.') {
      last_dot = p;
    } else if (is_url && *p == '?')
      break;
  }
  if (*last_dot == '.') {
    for (i = 1; i < 8 && last_dot[i]; i++) {
      if (is_url && !IS_ALNUM(last_dot[i]))
        break;
    }
    return allocStr(last_dot, i);
  } else
    return last_dot;
}

ParsedURL *schemaToProxy(UrlSchema schema) {
  ParsedURL *pu = NULL; /* for gcc */
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
#ifdef DEBUG
  default:
    abort();
#endif
  }
  return pu;
}

char *url_decode0(const char *url) {
  if (!DecodeURL)
    return (char *)url;
  return url_unquote_conv((char *)url, 0);
}

