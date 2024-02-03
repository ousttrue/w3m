#include "url_stream.h"
#include "loadproc.h"
#include "http_option.h"
#include "rc.h"
#include "app.h"
#include "alloc.h"
#include "screen.h"
#include "istream.h"
#include "indep.h"
#include "myctype.h"
#include "downloadlist.h"
#include "alloc.h"
#include "tmpfile.h"
#include "message.h"
#include "history.h"
#include "terms.h"
#include "signal_util.h"
#include "compression.h"
#include "mimetypes.h"
#include "ssl_util.h"
#include "local_cgi.h"
#include "form.h"
#include "etc.h"
#include "url.h"
#include "w3m.h"
#include "istream.h"
#include "httprequest.h"
#include "textlist.h"
#include "func.h"
#include "linein.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#include <sys/stat.h>

char *HTTP_proxy = nullptr;
char *HTTPS_proxy = nullptr;
char *FTP_proxy = nullptr;
char *NO_proxy = nullptr;
int NOproxy_netaddr = true;
bool use_proxy = true;
TextList *NO_proxy_domains;

Url HTTP_proxy_parsed;
Url HTTPS_proxy_parsed;
Url FTP_proxy_parsed;

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

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
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
bool PermitSaveToPipe = false;
bool AutoUncompress = false;
bool PreserveTimestamp = true;

// static JMP_BUF AbortLoading;
// static void KeyAbort(SIGNAL_ARG) {
//   LONGJMP(AbortLoading, 1);
//   SIGNAL_RETURN;
// }

char *index_file = nullptr;
bool use_lessopen = false;

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

void init_stream(UrlStream *uf, UrlSchema schema, input_stream *stream) {
  memset(uf, 0, sizeof(UrlStream));
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

/* add index_file if exists */
static void add_index_file(Url *pu, UrlStream *uf) {
  const char *p, *q;
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

static int dir_exist(const char *path) {
  struct stat stbuf;

  if (path == NULL || *path == '\0')
    return 0;
  if (stat(path, &stbuf) == -1)
    return 0;
  return IS_DIRECTORY(stbuf.st_mode);
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

UrlStream openURL(const char *url, Url *pu, Url *current,
                  const HttpOption &option, FormList *request,
                  TextList *extra_header, UrlStream *ouf, HRequest *hr,
                  unsigned char *status) {
  Str *tmp;
  int sock;
  const char *p, *q, *u;
  UrlStream uf;
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
  *pu = Url::parse2(u, current);
  if (pu->schema == SCM_LOCAL && pu->file == NULL) {
    if (pu->label != NULL) {
      /* #hogege is not a label but a filename */
      Str *tmp2 = Strnew_charp("#");
      Strcat_charp(tmp2, pu->label);
      pu->file = tmp2->ptr;
      pu->real_file = cleanupName(file_unquote((char *)pu->file));
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
  uf.url = Strnew(pu->to_Str())->ptr;
  pu->is_nocache = (option.no_cache);
  uf.ext = filename_extension(pu->file, 1);

  hr->method = HR_COMMAND_GET;
  hr->flag = {};
  hr->referer = option.referer;
  hr->request = request;

  switch (pu->schema) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    if (request && request->body)
      /* local CGI: POST */
      uf.stream = newFileStream(
          localcgi_post(pu->real_file, pu->query, request, option.referer),
          (void (*)())fclose);
    else
      /* lodal CGI: GET */
      uf.stream =
          newFileStream(localcgi_get(pu->real_file, pu->query, option.referer),
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
      hr->method = HR_COMMAND_POST;
    if (request && request->method == FORM_METHOD_HEAD)
      hr->method = HR_COMMAND_HEAD;
    if (((pu->schema == SCM_HTTPS) ? non_null(HTTPS_proxy)
                                   : non_null(HTTP_proxy)) &&
        use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
      hr->flag = (HttpRequestFlags)(hr->flag | HR_FLAG_PROXY);
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
          hr->method = HR_COMMAND_CONNECT;
          tmp = HTTPrequest(pu, current, hr, extra_header);
          *status = HTST_CONNECT;
        } else {
          hr->flag = (HttpRequestFlags)(hr->flag | HR_FLAG_LOCAL);
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
      hr->flag = (HttpRequestFlags)(hr->flag | HR_FLAG_LOCAL);
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
      if (hr->method == HR_COMMAND_POST &&
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
      if (hr->method == HR_COMMAND_POST &&
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

static FILE *lessopen_stream(const char *path) {

  auto lessopen = getenv("LESSOPEN");
  if (lessopen == NULL || lessopen[0] == '\0')
    return NULL;

  if (lessopen[0] != '|') /* filename mode, not supported m(__)m */
    return NULL;

  /* pipe mode */
  ++lessopen;

  /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
  int n = 0;
  for (const char *f = lessopen; *f; f++) {
    if (*f == '%') {
      if (f[1] == '%') /* Literal % */
        f++;
      else if (*++f == 's') {
        if (n)
          return NULL;
        n++;
      } else
        return NULL;
    }
  }
  if (!n)
    return NULL;

  auto tmpf = Sprintf(lessopen, shell_quote((char *)path));
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

void examineFile(const char *path, UrlStream *uf) {
  struct stat stbuf;

  uf->guess_type = NULL;
  if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 ||
      NOT_REGULAR(stbuf.st_mode)) {
    uf->stream = NULL;
    return;
  }
  uf->stream = openIS(path);
  if (!do_download) {
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
      auto ext = uf->ext;
      auto t0 = uncompressed_file_type(path, &ext);
      uf->guess_type = t0;
      uf->ext = ext;
      uncompress_stream(uf, NULL);
      return;
    }
  }
}

int save2tmp(UrlStream *uf, const char *tmpf) {
  FILE *ff;
  long long linelen = 0;
  // long long trbyte = 0;
  MySignalHandler prevtrap = NULL;
  static JMP_BUF env_bak;
  int retval = 0;
  char *buf = NULL;

  ff = fopen(tmpf, "wb");
  if (ff == NULL) {
    /* fclose(f); */
    return -1;
  }
  bcopy(AbortLoading, env_bak, sizeof(JMP_BUF));
  if (SETJMP(AbortLoading) != 0) {
    goto _end;
  }
  TRAP_ON;
  {
    int count;
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
      if (static_cast<int>(fwrite(buf, 1, count, ff)) != count) {
        retval = -2;
        goto _end;
      }
      linelen += count;
      // showProgress(&linelen, &trbyte, current_content_length);
    }
  }
_end:
  bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
  TRAP_OFF;
  xfree(buf);
  fclose(ff);
  // current_content_length = 0;
  return retval;
}

int checkSaveFile(input_stream *stream, const char *path2) {
  struct stat st1, st2;
  int des = ISfileno(stream);

  if (des < 0)
    return 0;
  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int doFileSave(UrlStream *uf, const char *defstr) {
  Str *msg;
  Str *filen;
  const char *p, *q;
  pid_t pid;
  char *lock;
  const char *tmpf = NULL;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      // p = inputLineHist("(Download)Save file to: ", defstr, IN_FILENAME,
      //                   SaveHist);
      if (p == NULL || *p == '\0')
        return -1;
    }
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(uf->stream, p) < 0) {
      msg = Sprintf("Can't save. Load file and %s are identical.", p);
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    symlink(p, lock);

    flush_tty();
    pid = fork();
    if (!pid) {
      int err;
      if ((uf->content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(uf, &tmpf);
        if (tmpf)
          unlink(tmpf);
      }
      setup_child(FALSE, 0, UFfileno(uf));
      err = save2tmp(uf, p);
      if (err == 0 && PreserveTimestamp && uf->modtime != -1)
        setModtime(p, uf->modtime);
      UFclose(uf);
      unlink(lock);
      if (err != 0)
        exit(-err);
      exit(0);
    }
    addDownloadList(pid, (char *)uf->url, p, lock,
                    0 /*current_content_length*/);
  } else {
    q = searchKeyData();
    if (q == NULL || *q == '\0') {
      /* FIXME: gettextize? */
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(char *)(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = expandPath((char *)q);
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(uf->stream, p) < 0) {
      printf("Can't save. Load file and %s are identical.", p);
      return -1;
    }
    if (uf->content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
      uncompress_stream(uf, &tmpf);
      if (tmpf)
        unlink(tmpf);
    }
    if (save2tmp(uf, p) < 0) {
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && uf->modtime != -1)
      setModtime(p, uf->modtime);
  }
  return 0;
}

void UFhalfclose(UrlStream *f) {
  switch (f->schema) {
  case SCM_FTP:
    // closeFTP();
    break;
  default:
    UFclose(f);
    break;
  }
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
