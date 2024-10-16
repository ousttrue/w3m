#include "input/url_stream.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "core.h"
#include "file/file.h"
#include "file/shell.h"
#include "fm.h"
// #include "html/html.h"
#include "input/ftp.h"
#include "input/http_cookie.h"
#include "input/http_request.h"
#include "input/isocket.h"
#include "input/istream.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "rand48.h"
#include "rc.h"
#include "siteconf.h"
#include "term/terms.h"
#include "text/Str.h"
#include "text/myctype.h"
#include "text/regex.h"
#include "text/text.h"
#include "textlist.h"
#include "trap_jmp.h"

#include <assert.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <sys/stat.h>
#ifdef _WIN32
#else
#endif

#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>

Str header_string = NULL;

struct TextList *NO_proxy_domains = nullptr;
void url_stream_init() { NO_proxy_domains = newTextList(); }

/* XXX: note html.h SCM_ */
static int DefaultPort[] = {
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

static struct {
  const char *ext;
  const char *mime;
} DefaultGuess[] = {
    {"html", "text/html"},         {"htm", "text/html"},
    {"shtml", "text/html"},        {"xhtml", "application/xhtml+xml"},
    {"gif", "image/gif"},          {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},         {"png", "image/png"},
    {"xbm", "image/xbm"},          {"au", "audio/basic"},
    {"gz", "application/x-gzip"},  {"Z", "application/x-compress"},
    {"bz2", "application/x-bzip"}, {"tar", "application/x-tar"},
    {"zip", "application/x-zip"},  {"lha", "application/x-lha"},
    {"lzh", "application/x-lha"},  {"ps", "application/postscript"},
    {"pdf", "application/pdf"},    {NULL, NULL}};

static void add_index_file(struct Url *pu, struct URLFile *uf);

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

static struct TextList *mimetypes_list = nullptr;
struct table2 {
  const char* item1;
  const char* item2;
};
static struct table2 **UserMimeTypes = nullptr;

static struct table2 *loadMimeTypes(char *filename) {
  auto f = fopen(expandPath(filename), "r");
  if (f == NULL)
    return NULL;

  int n = 0;
  Str tmp;
  while (tmp = Strfgets(f), tmp->length > 0) {
    auto d = tmp->ptr;
    if (d[0] != '#') {
      d = strtok(d, " \t\n\r");
      if (d != NULL) {
        d = strtok(NULL, " \t\n\r");
        int i = 0;
        for (; d != NULL; i++)
          d = strtok(NULL, " \t\n\r");
        n += i;
      }
    }
  }

  fseek(f, 0, 0);
  auto mtypes = New_N(struct table2, n + 1);
  int i = 0;
  while (tmp = Strfgets(f), tmp->length > 0) {
    auto d = tmp->ptr;
    if (d[0] == '#')
      continue;
    auto type = strtok(d, " \t\n\r");
    if (type == NULL)
      continue;
    while (1) {
      d = strtok(NULL, " \t\n\r");
      if (d == NULL)
        break;
      mtypes[i].item1 = Strnew_charp(d)->ptr;
      mtypes[i].item2 = Strnew_charp(type)->ptr;
      i++;
    }
  }

  mtypes[i].item1 = NULL;
  mtypes[i].item2 = NULL;
  fclose(f);
  return mtypes;
}

void initMimeTypes() {
  int i;
  struct TextListItem *tl;

  if (non_null(mimetypes_files))
    mimetypes_list = make_domain_list(mimetypes_files);
  else
    mimetypes_list = NULL;
  if (mimetypes_list == NULL)
    return;
  UserMimeTypes = New_N(struct table2 *, mimetypes_list->nitem);
  for (i = 0, tl = mimetypes_list->first; tl; i++, tl = tl->next)
    UserMimeTypes[i] = loadMimeTypes(tl->ptr);
}

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

SSL_CTX *ssl_ctx = NULL;

void free_ssl_ctx() {
  if (ssl_ctx != NULL)
    SSL_CTX_free(ssl_ctx);
  ssl_ctx = NULL;
  ssl_accept_this_site(NULL);
}

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void init_PRNG() {
  if (RAND_status())
    return;

  char buffer[256];
  const char *file = nullptr;
  if ((file = RAND_file_name(buffer, sizeof(buffer)))) {
#ifdef USE_EGD
    if (RAND_egd(file) > 0)
      return;
#endif
    RAND_load_file(file, -1);
  }
  if (RAND_status())
    goto seeded;
  srand48((long)time(NULL));
  while (!RAND_status()) {
    long l = lrand48();
    RAND_seed((unsigned char *)&l, sizeof(long));
  }
seeded:
  if (file)
    RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

#ifdef SSL_CTX_set_min_proto_version
static int str_to_ssl_version(const char *name) {
  if (!strcasecmp(name, "all"))
    return 0;
  if (!strcasecmp(name, "none"))
    return 0;
#ifdef TLS1_3_VERSION
  if (!strcasecmp(name, "TLSv1.3"))
    return TLS1_3_VERSION;
#endif
#ifdef TLS1_2_VERSION
  if (!strcasecmp(name, "TLSv1.2"))
    return TLS1_2_VERSION;
#endif
#ifdef TLS1_1_VERSION
  if (!strcasecmp(name, "TLSv1.1"))
    return TLS1_1_VERSION;
#endif
  if (!strcasecmp(name, "TLSv1.0"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "TLSv1"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "SSLv3.0"))
    return SSL3_VERSION;
  if (!strcasecmp(name, "SSLv3"))
    return SSL3_VERSION;
  return -1;
}
#endif /* SSL_CTX_set_min_proto_version */

static SSL *openSSLHandle(int sock, char *hostname, char **p_cert) {
  SSL *handle = NULL;
  static char *old_ssl_forbid_method = NULL;
  static int old_ssl_verify_server = -1;

  if (old_ssl_forbid_method != ssl_forbid_method &&
      (!old_ssl_forbid_method || !ssl_forbid_method ||
       strcmp(old_ssl_forbid_method, ssl_forbid_method))) {
    old_ssl_forbid_method = ssl_forbid_method;
    ssl_path_modified = 1;
  }
  if (old_ssl_verify_server != ssl_verify_server) {
    old_ssl_verify_server = ssl_verify_server;
    ssl_path_modified = 1;
  }
  if (ssl_path_modified) {
    free_ssl_ctx();
    ssl_path_modified = 0;
  }
  if (ssl_ctx == NULL) {
    int option;
#if OPENSSL_VERSION_NUMBER < 0x0800
    ssl_ctx = SSL_CTX_new();
    X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(0, NULL);
#endif
    if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
      goto eend;
#ifdef SSL_CTX_set_min_proto_version
    if (ssl_min_version && *ssl_min_version != '\0') {
      int sslver;
      sslver = str_to_ssl_version(ssl_min_version);
      if (sslver < 0 || !SSL_CTX_set_min_proto_version(ssl_ctx, sslver)) {
        free_ssl_ctx();
        goto eend;
      }
    }
#endif
    if (ssl_cipher && *ssl_cipher != '\0')
      if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
        free_ssl_ctx();
        goto eend;
      }
    option = SSL_OP_ALL;
    if (ssl_forbid_method) {
      if (strchr(ssl_forbid_method, '2'))
        option |= SSL_OP_NO_SSLv2;
      if (strchr(ssl_forbid_method, '3'))
        option |= SSL_OP_NO_SSLv3;
      if (strchr(ssl_forbid_method, 't'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, 'T'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, '4'))
        option |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
      if (strchr(ssl_forbid_method, '5'))
        option |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
      if (strchr(ssl_forbid_method, '6'))
        option |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
      if (strchr(ssl_forbid_method, '7'))
        option |= SSL_OP_NO_TLSv1_3;
#endif
    }
#ifdef SSL_OP_NO_COMPRESSION
    option |= SSL_OP_NO_COMPRESSION;
#endif
    SSL_CTX_set_options(ssl_ctx, option);

#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

    /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(
        ssl_ctx, ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
    if (ssl_cert_file != NULL && *ssl_cert_file != '\0') {
      int ng = 1;
      if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file,
                                       SSL_FILETYPE_PEM) > 0) {
        char *key_file = (ssl_key_file == NULL || *ssl_key_file == '\0')
                             ? ssl_cert_file
                             : ssl_key_file;
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) >
            0)
          if (SSL_CTX_check_private_key(ssl_ctx))
            ng = 0;
      }
      if (ng) {
        free_ssl_ctx();
        goto eend;
      }
    }
    if (ssl_verify_server) {
      char *file = NULL, *path = NULL;
      if (ssl_ca_file && *ssl_ca_file != '\0')
        file = ssl_ca_file;
      if (ssl_ca_path && *ssl_ca_path != '\0')
        path = ssl_ca_path;
      if ((file || path) &&
          !SSL_CTX_load_verify_locations(ssl_ctx, file, path)) {
        free_ssl_ctx();
        goto eend;
      }
      if (ssl_ca_default)
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    }
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
  }
  handle = SSL_new(ssl_ctx);
  SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
  init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
  SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) \
        */
  if (SSL_connect(handle) > 0) {
    Str serv_cert = ssl_get_certificate(handle, hostname);
    if (serv_cert) {
      *p_cert = serv_cert->ptr;
      return handle;
    }
    close(sock);
    SSL_free(handle);
    return NULL;
  }
eend:
  close(sock);
  if (handle)
    SSL_free(handle);
  /* FIXME: gettextize? */
  disp_err_message(
      Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
              ERR_error_string(ERR_get_error(), NULL))
          ->ptr,
      false);
  return NULL;
}

static void SSL_write_from_file(SSL *ssl, const char *file) {
  auto fd = fopen(file, "r");
  if (fd != NULL) {
    int c;
    while ((c = fgetc(fd)) != EOF) {
      char buf[1];
      buf[0] = c;
      SSL_write(ssl, buf, 1);
    }
    fclose(fd);
  }
}

static void write_from_file(int sock, const char *file) {
  auto fd = fopen(file, "r");
  if (fd != NULL) {
    int c;
    while ((c = fgetc(fd)) != EOF) {
      char buf[1];
      buf[0] = c;
      write(sock, buf, 1);
    }
    fclose(fd);
  }
}

struct Url *baseURL(struct Buffer *buf) {
  if (buf->bufferprop & BP_NO_URL) {
    /* no URL is defined for the buffer */
    return NULL;
  }

  if (buf->document->baseURL != NULL) {
    /* <BASE> tag is defined in the document */
    return buf->document->baseURL;
  } else if (IS_EMPTY_PARSED_URL(&buf->currentURL))
    return NULL;
  else
    return &buf->currentURL;
}

#define COPYPATH_SPC_ALLOW 0
#define COPYPATH_SPC_IGNORE 1
#define COPYPATH_SPC_REPLACE 2
#define COPYPATH_SPC_MASK 3
#define COPYPATH_LOWERCASE 4

static char *copyPath(const char *orgpath, int length, int option) {
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

#define ALLOC_STR(s) ((s) == NULL ? NULL : allocStr(s, -1))

void copyParsedURL(struct Url *p, const struct Url *q) {
  if (q == NULL) {
    memset(p, 0, sizeof(struct Url));
    p->scheme = SCM_UNKNOWN;
    return;
  }
  p->scheme = q->scheme;
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

static const char *scheme_str[] = {
    "http", "gopher", "ftp",  "ftp",  "file", "file",   "exec",
    "nntp", "nntp",   "news", "news", "data", "mailto", "https",
};

static Str _parsedURL2Str(struct Url *pu, int pass, int user, int label) {
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

Str parsedURL2Str(struct Url *pu) {
  return _parsedURL2Str(pu, false, true, true);
}

static Str parsedURL2RefererOriginStr(struct Url *pu) {
  Str s;
  const char *f = pu->file, *q = pu->query;

  pu->file = NULL;
  pu->query = NULL;
  s = _parsedURL2Str(pu, false, false, false);
  pu->file = f;
  pu->query = q;

  return s;
}

Str parsedURL2RefererStr(struct Url *pu) {
  return _parsedURL2Str(pu, false, false, false);
}

static char *otherinfo(struct Url *target, struct Url *current,
                       const char *referer) {
  Str s = Strnew();
  const int *no_referer_ptr;
  int no_referer;
  const char *url_user_agent = query_SCONF_USER_AGENT(target);

  if (!override_user_agent) {
    Strcat_charp(s, "User-Agent: ");
    if (url_user_agent)
      Strcat_charp(s, url_user_agent);
    else if (UserAgent == NULL || *UserAgent == '\0')
      Strcat_charp(s, w3m_version);
    else
      Strcat_charp(s, UserAgent);
    Strcat_charp(s, "\r\n");
  }

  Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", NULL);
  Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", NULL);
  Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", NULL);

  if (target->host) {
    Strcat_charp(s, "Host: ");
    Strcat_charp(s, target->host);
    if (target->port != DefaultPort[target->scheme])
      Strcat(s, Sprintf(":%d", target->port));
    Strcat_charp(s, "\r\n");
  }
  if (target->is_nocache || NoCache) {
    Strcat_charp(s, "Pragma: no-cache\r\n");
    Strcat_charp(s, "Cache-control: no-cache\r\n");
  }
  no_referer = NoSendReferer;
  no_referer_ptr = query_SCONF_NO_REFERER_FROM(current);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  no_referer_ptr = query_SCONF_NO_REFERER_TO(target);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  if (!no_referer) {
    int cross_origin = false;
    if (CrossOriginReferer && current && current->host &&
        (!target || !target->host ||
         strcasecmp(current->host, target->host) != 0 ||
         current->port != target->port || current->scheme != target->scheme))
      cross_origin = true;
    if (current && current->scheme == SCM_HTTPS &&
        target->scheme != SCM_HTTPS) {
      /* Don't send Referer: if https:// -> http:// */
    } else if (referer == NULL && current && current->scheme != SCM_LOCAL &&
               current->scheme != SCM_LOCAL_CGI &&
               current->scheme != SCM_DATA &&
               (current->scheme != SCM_FTP ||
                (current->user == NULL && current->pass == NULL))) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, parsedURL2RefererOriginStr(current));
      else
        Strcat(s, parsedURL2RefererStr(current));
      Strcat_charp(s, "\r\n");
    } else if (referer != NULL && referer != NO_REFERER) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, parsedURL2RefererOriginStr(current));
      else
        Strcat_charp(s, referer);
      Strcat_charp(s, "\r\n");
    }
  }
  return s->ptr;
}

Str HTTPrequestMethod(struct HttpRequest *hr) {
  switch (hr->command) {
  case HR_COMMAND_CONNECT:
    return Strnew_charp("CONNECT");
  case HR_COMMAND_POST:
    return Strnew_charp("POST");
    break;
  case HR_COMMAND_HEAD:
    return Strnew_charp("HEAD");
    break;
  case HR_COMMAND_GET:
  default:
    return Strnew_charp("GET");
  }
  return NULL;
}

Str HTTPrequestURI(struct Url *pu, struct HttpRequest *hr) {
  Str tmp = Strnew();
  if (hr->command == HR_COMMAND_CONNECT) {
    Strcat_charp(tmp, pu->host);
    Strcat(tmp, Sprintf(":%d", pu->port));
  } else if (hr->flag & HR_FLAG_LOCAL) {
    Strcat_charp(tmp, pu->file);
    if (pu->query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, pu->query);
    }
  } else
    Strcat(tmp, _parsedURL2Str(pu, true, true, false));
  return tmp;
}

static Str HTTPrequest(struct Url *pu, struct Url *current,
                       struct HttpRequest *hr, struct TextList *extra) {
  Str tmp;
  struct TextListItem *i;
  Str cookie;
  tmp = HTTPrequestMethod(hr);
  Strcat_charp(tmp, " ");
  Strcat_charp(tmp, HTTPrequestURI(pu, hr)->ptr);
  Strcat_charp(tmp, " HTTP/1.0\r\n");
  if (hr->referer == NO_REFERER)
    Strcat_charp(tmp, otherinfo(pu, NULL, NULL));
  else
    Strcat_charp(tmp, otherinfo(pu, current, hr->referer));
  if (extra != NULL)
    for (i = extra->first; i != NULL; i = i->next) {
      if (strncasecmp(i->ptr, "Authorization:", sizeof("Authorization:") - 1) ==
          0) {
        if (hr->command == HR_COMMAND_CONNECT)
          continue;
      }
      if (strncasecmp(i->ptr, "Proxy-Authorization:",
                      sizeof("Proxy-Authorization:") - 1) == 0) {
        if (pu->scheme == SCM_HTTPS && hr->command != HR_COMMAND_CONNECT)
          continue;
      }
      Strcat_charp(tmp, i->ptr);
    }

  if (hr->command != HR_COMMAND_CONNECT && use_cookie &&
      (cookie = find_cookie(pu))) {
    Strcat_charp(tmp, "Cookie: ");
    Strcat(tmp, cookie);
    Strcat_charp(tmp, "\r\n");
    /* [DRAFT 12] s. 10.1 */
    if (cookie->ptr[0] != '$')
      Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
  }
  if (hr->command == HR_COMMAND_POST) {
    if (hr->request->enctype == FORM_ENCTYPE_MULTIPART) {
      Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
      Strcat_charp(tmp, hr->request->boundary);
      Strcat_charp(tmp, "\r\n");
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->request->length));
      Strcat_charp(tmp, "\r\n");
    } else {
      if (!override_content_type) {
        Strcat_charp(tmp,
                     "Content-Type: application/x-www-form-urlencoded\r\n");
      }
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->request->length));
      if (header_string)
        Strcat(tmp, header_string);
      Strcat_charp(tmp, "\r\n");
      Strcat_charp_n(tmp, hr->request->body, hr->request->length);
      Strcat_charp(tmp, "\r\n");
    }
  } else {
    if (header_string)
      Strcat(tmp, header_string);
    Strcat_charp(tmp, "\r\n");
  }
#ifdef DEBUG
  fprintf(stderr, "HTTPrequest: [ %s ]\n\n", tmp->ptr);
#endif /* DEBUG */
  return tmp;
}

void init_stream(struct URLFile *uf, int scheme, union input_stream *stream) {
  memset(uf, 0, sizeof(struct URLFile));
  uf->stream = stream;
  uf->scheme = scheme;
  uf->encoding = ENC_7BIT;
  uf->is_cgi = false;
  uf->compression = CMP_NOCOMPRESS;
  uf->content_encoding = CMP_NOCOMPRESS;
  uf->guess_type = NULL;
  uf->ext = NULL;
  uf->modtime = -1;
}

struct URLFile openURL(const char *url, struct Url *pu, struct Url *current,
                       struct URLOption *option, struct FormList *request,
                       struct TextList *extra_header, struct URLFile *ouf,
                       struct HttpRequest *hr, enum HttpStatus *status) {
  struct HttpRequest hr0;
  if (!hr) {
    hr = &hr0;
  }

  struct URLFile uf;
  if (ouf) {
    uf = *ouf;
  } else {
    init_stream(&uf, SCM_MISSING, NULL);
  }

  auto u = url;
  auto scheme = getURLScheme(&u);
  if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL) {
    u = file_to_url(url); /* force to local file */
  } else {
    u = url;
  }

  parseURL2(u, pu, current);
  if (pu->scheme == SCM_LOCAL && pu->file == NULL) {
    if (pu->label != NULL) {
      /* #hogege is not a label but a filename */
      Str tmp2 = Strnew_charp("#");
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

  if (LocalhostOnly && pu->host && !is_localhost(pu->host)) {
    pu->host = NULL;
  }

  uf.scheme = pu->scheme;
  uf.url = parsedURL2Str(pu)->ptr;
  pu->is_nocache = (option->flag & RG_NOCACHE);
  uf.ext = filename_extension(pu->file, 1);

  hr->command = HR_COMMAND_GET;
  hr->flag = 0;
  hr->referer = option->referer;
  hr->request = request;

  switch (pu->scheme) {
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
      uf.is_cgi = true;
      uf.scheme = pu->scheme = SCM_LOCAL_CGI;
      return uf;
    }

    examineFile(pu->real_file, &uf);
    if (uf.stream == NULL) {
      if (dir_exist(pu->real_file)) {
        add_index_file(pu, &uf);
        if (uf.stream == NULL)
          return uf;
      } else if (document_root != NULL) {
        auto tmp = Strnew_charp(document_root);
        if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
          Strcat_char(tmp, '/');
        Strcat_charp(tmp, pu->file);
        auto p = cleanupName(tmp->ptr);
        auto q = cleanupName(file_unquote(p));
        if (dir_exist(q)) {
          pu->file = p;
          pu->real_file = q;
          add_index_file(pu, &uf);
          if (uf.stream == NULL) {
            return uf;
          }
        } else {
          examineFile(q, &uf);
          if (uf.stream) {
            pu->file = p;
            pu->real_file = q;
          }
        }
      }
    }
    return uf;

  case SCM_FTP:
  case SCM_FTPDIR:
    if (pu->file == NULL)
      pu->file = allocStr("/", -1);
    if (non_null(FTP_proxy) && use_proxy && pu->host != NULL &&
        !check_no_proxy(pu->host)) {
      hr->flag |= HR_FLAG_PROXY;
      SocketType sock;
      if (!socketOpen(FTP_proxy_parsed.host,
                      schemeNumToName(FTP_proxy_parsed.scheme),
                      FTP_proxy_parsed.port, &sock)) {
        return uf;
      }
#ifdef _WIN32
      assert(false);
#else
      uf.stream = newInputStream(sock);
#endif
      uf.scheme = SCM_HTTP;
      auto tmp = HTTPrequest(pu, current, hr, extra_header);
      socketWrite(sock, tmp->ptr, tmp->length);
    } else {
      uf.stream = openFTPStream(pu, &uf);
      uf.scheme = pu->scheme;
      return uf;
    }
    break;

  case SCM_HTTP:
  case SCM_HTTPS: {
    if (pu->file == NULL)
      pu->file = allocStr("/", -1);
    if (request && request->method == FORM_METHOD_POST && request->body)
      hr->command = HR_COMMAND_POST;
    if (request && request->method == FORM_METHOD_HEAD)
      hr->command = HR_COMMAND_HEAD;

    Str tmp = nullptr;
    SocketType sock = socketInvalid();
    SSL *sslh = NULL;
    if (((pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy)
                                   : non_null(HTTP_proxy)) &&
        use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
      hr->flag |= HR_FLAG_PROXY;
      if (pu->scheme == SCM_HTTPS && *status == HTST_CONNECT) {
        sock = ssl_socket_of(ouf->stream);
        if (!(sslh = openSSLHandle(sock, pu->host, &uf.ssl_certificate))) {
          *status = HTST_MISSING;
          return uf;
        }
      } else if (pu->scheme == SCM_HTTPS) {
        if (!socketOpen(HTTPS_proxy_parsed.host,
                        schemeNumToName(HTTPS_proxy_parsed.scheme),
                        HTTPS_proxy_parsed.port, &sock)) {
#ifdef SOCK_DEBUG
          sock_log("Can't open socket\n");
#endif
          return uf;
        }
        sslh = NULL;
      } else {
        if (!socketOpen(HTTP_proxy_parsed.host,
                        schemeNumToName(HTTP_proxy_parsed.scheme),
                        HTTP_proxy_parsed.port, &sock)) {
#ifdef SOCK_DEBUG
          sock_log("Can't open socket\n");
#endif
          return uf;
        }
        sslh = NULL;
      }
      if (pu->scheme == SCM_HTTPS) {
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
      if (!socketOpen(pu->host, schemeNumToName(pu->scheme), pu->port, &sock)) {
        *status = HTST_MISSING;
        return uf;
      }
      if (pu->scheme == SCM_HTTPS) {
        if (!(sslh = openSSLHandle(sock, pu->host, &uf.ssl_certificate))) {
          *status = HTST_MISSING;
          return uf;
        }
      }
      hr->flag |= HR_FLAG_LOCAL;
      tmp = HTTPrequest(pu, current, hr, extra_header);
      *status = HTST_NORMAL;
    }
#ifdef _WIN32
    uf.stream = newWinsockStream(sock);
#else
    uf.stream = newInputStream(sock);
#endif
    if (pu->scheme == SCM_HTTPS) {
      uf.stream = newSSLStream(sslh, sock);
      if (sslh)
        SSL_write(sslh, tmp->ptr, tmp->length);
      else
        socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        if (sslh)
          SSL_write_from_file(sslh, request->body);
        else
          write_from_file(sock, request->body);
      }
      return uf;
    } else {
      socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART)
        write_from_file(sock, request->body);
    }
    break;
  }

  case SCM_DATA: {
    if (pu->file == NULL)
      return uf;
    auto p = Strnew_charp(pu->file)->ptr;
    auto q = strchr(p, ',');
    if (q == NULL)
      return uf;
    *q++ = '\0';
    auto tmp = Strnew_charp(q);
    q = strrchr(p, ';');
    if (q != NULL && !strcmp(q, ";base64")) {
      *q = '\0';
      uf.encoding = ENC_BASE64;
    } else
      tmp = Str_url_unquote(tmp, false, false);
    uf.stream = newStrStream(tmp);
    uf.guess_type = (*p != '\0') ? p : "text/plain";
    return uf;
  }
  case SCM_UNKNOWN:
  default:
    return uf;
  }
  return uf;
}

/* add index_file if exists */
static void add_index_file(struct Url *pu, struct URLFile *uf) {
  const char *p, *q;
  struct TextList *index_file_list = NULL;
  struct TextListItem *ti;

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

static char *guessContentTypeFromTable(struct table2 *table,
                                       const char *filename) {
  if (!table)
    return NULL;
  auto p = &filename[strlen(filename) - 1];
  while (filename < p && *p != '.')
    p--;
  if (p == filename)
    return NULL;
  p++;
  for (auto t = table; t->item1; t++) {
    if (!strcmp(p, t->item1))
      return t->item2;
  }
  for (auto t = table; t->item1; t++) {
    if (!strcasecmp(p, t->item1))
      return t->item2;
  }
  return NULL;
}

const char *guessContentType(const char *filename) {
  char *ret;
  int i;

  if (filename == NULL)
    return NULL;
  if (mimetypes_list == NULL)
    goto no_user_mimetypes;

  for (i = 0; i < mimetypes_list->nitem; i++) {
    if ((ret = guessContentTypeFromTable(UserMimeTypes[i], filename)) != NULL)
      return ret;
  }

no_user_mimetypes:
  return guessContentTypeFromTable(DefaultGuess, filename);
}

struct TextList *make_domain_list(char *domain_list) {
  char *p;
  Str tmp;
  struct TextList *domains = NULL;

  p = domain_list;
  tmp = Strnew_size(64);
  while (*p) {
    while (*p && IS_SPACE(*p))
      p++;
    Strclear(tmp);
    while (*p && !IS_SPACE(*p) && *p != ',')
      Strcat_char(tmp, *p++);
    if (tmp->length > 0) {
      if (domains == NULL)
        domains = newTextList();
      pushText(domains, tmp->ptr);
    }
    while (*p && IS_SPACE(*p))
      p++;
    if (*p == ',')
      p++;
  }
  return domains;
}

static int domain_match(char *pat, char *domain) {
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

int check_no_proxy(char *domain) {
  struct TextListItem *tl;
  volatile int ret = 0;

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
  if (from_jmp()) {
    ret = 0;
    goto end;
  }
  trap_on();
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
  trap_off();
  return ret;
}

struct Url *schemeToProxy(enum URL_SCHEME_TYPE scheme) {
  struct Url *pu = NULL; /* for gcc */
  switch (scheme) {
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
    abort();
  }
  return pu;
}

void UFhalfclose(struct URLFile *f) {
  switch (f->scheme) {
  case SCM_FTP:
    closeFTP();
    break;
  default:
    UFclose(f);
    break;
  }
}

bool is_text_type(const char *type) {
  return (type == NULL || type[0] == '\0' ||
          strncasecmp(type, "text/", 5) == 0 ||
          (strncasecmp(type, "application/", 12) == 0 &&
           strstr(type, "xhtml") != NULL) ||
          strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

bool is_plain_text_type(const char *type) {
  return ((type && strcasecmp(type, "text/plain") == 0) ||
          (is_text_type(type)));
}

bool is_html_type(const char *type) {
  return (type && (strcasecmp(type, "text/html") == 0 ||
                   strcasecmp(type, "application/xhtml+xml") == 0));
}

static FILE *lessopen_stream(const char *path) {

  auto lessopen = getenv("LESSOPEN");
  if (lessopen == NULL) {
    return NULL;
  }
  if (lessopen[0] == '\0') {
    return NULL;
  }

  if (lessopen[0] != '|') {
    /* filename mode */
    /* not supported m(__)m */
    return nullptr;
  }

  /* pipe mode */
  ++lessopen;
  auto tmpf = Sprintf(lessopen, shell_quote(path));
  auto fp = popen(tmpf->ptr, "r");
  if (fp == NULL) {
    return NULL;
  }
  auto c = getc(fp);
  if (c == EOF) {
    pclose(fp);
    return NULL;
  }
  ungetc(c, fp);
  return fp;
}

void examineFile(const char *path, struct URLFile *uf) {
  uf->guess_type = NULL;
  struct stat stbuf;
  if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 ||
      NOT_REGULAR(stbuf.st_mode)) {
    uf->stream = NULL;
    return;
  }
  uf->stream = openIS(path);

  if (use_lessopen && getenv("LESSOPEN") != NULL) {
    FILE *fp;
    uf->guess_type = guessContentType(path);
    if (uf->guess_type == NULL)
      uf->guess_type = "text/plain";
    if (is_html_type(uf->guess_type))
      return;
    if ((fp = lessopen_stream(path))) {
      UFclose(uf);
      uf->stream = newFileStream(fp, (void (*)())pclose);
      uf->guess_type = "text/plain";
      return;
    }
  }
  check_compression(path, uf);
  if (uf->compression != CMP_NOCOMPRESS) {
    const char *ext = uf->ext;
    auto t0 = uncompressed_file_type(path, &ext);
    uf->guess_type = t0;
    uf->ext = ext;
    uncompress_stream(uf, NULL);
    return;
  }
}
