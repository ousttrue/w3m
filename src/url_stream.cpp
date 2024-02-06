#include "url_stream.h"
#include "loadproc.h"
#include "http_option.h"
#include "rc.h"
#include "app.h"
#include "alloc.h"
#include "screen.h"
#include "istream.h"
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

#ifdef SOCK_DEBUG
#include <stdarg.h>

static void sock_log(char *message, ...) {
  FILE *f = fopen("zzzsocklog", "a");
  va_list va;

  if (f == nullptr)
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

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
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

/* add index_file if exists */
void UrlStream::add_index_file(Url *pu) {
  TextList *index_file_list = nullptr;
  if (non_null(index_file)) {
    index_file_list = make_domain_list(index_file);
  }
  if (!index_file_list) {
    this->stream = nullptr;
    return;
  }

  for (auto ti = index_file_list->first; ti; ti = ti->next) {
    const char *p =
        Strnew_m_charp(pu->file.c_str(), "/", file_quote(ti->ptr), nullptr)
            ->ptr;
    p = cleanupName(p);
    auto q = cleanupName(file_unquote(p));
    this->openFile(q);
    if (this->stream) {
      pu->file = p;
      pu->real_file = q;
      return;
    }
  }
}

static int dir_exist(const char *path) {
  struct stat stbuf;

  if (path == nullptr || *path == '\0')
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
  if (fd != nullptr) {
    while ((c = fgetc(fd)) != EOF) {
      buf[0] = c;
      write(sock, buf, 1);
    }
    fclose(fd);
  }
}

void UrlStream::openLocalCgi(Url *pu, std::optional<Url> current,
                             const HttpOption &option, FormList *request,
                             TextList *extra_header, HttpRequest *hr) {
  if (request && request->body)
    /* local CGI: POST */
    this->stream =
        newFileStream(localcgi_post(pu->real_file.c_str(), pu->query.c_str(),
                                    request, option.referer),
                      (void (*)())fclose);
  else
    /* lodal CGI: GET */
    this->stream = newFileStream(
        localcgi_get(pu->real_file.c_str(), pu->query.c_str(), option.referer),
        (void (*)())fclose);
  if (this->stream) {
    this->is_cgi = true;
    this->schema = pu->schema = SCM_LOCAL_CGI;
    return;
  }

  this->openFile(pu->real_file.c_str());
  if (this->stream == nullptr) {
    if (dir_exist(pu->real_file.c_str())) {
      add_index_file(pu);
      if (!this->stream) {
        return;
      }
    } else if (document_root != nullptr) {
      auto tmp = Strnew_charp(document_root);
      if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
        Strcat_char(tmp, '/');
      Strcat(tmp, pu->file);
      auto p = cleanupName(tmp->ptr);
      auto q = cleanupName(file_unquote(p));
      if (dir_exist(q)) {
        pu->file = p;
        pu->real_file = (char *)q;
        add_index_file(pu);
        if (!this->stream) {
          return;
        }
      } else {
        this->openFile(q);
        if (this->stream) {
          pu->file = p;
          pu->real_file = (char *)q;
        }
      }
    }
  }

  if (this->stream == nullptr && retryAsHttp && url[0] != '/') {
    auto u = url;
    auto schema = parseUrlSchema(&u);
    if (schema == SCM_MISSING || schema == SCM_UNKNOWN) {
      /* retry it as "http://" */
      u = Strnew_m_charp("http://", url, nullptr)->ptr;
      openURL(u, pu, current, option, request, extra_header, hr);
    }
  }
}

static int openSocket(const char *const hostname, const char *remoteport_name,
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
  MySignalHandler prevtrap = nullptr;

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
  if (hostname == nullptr) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n", hostname);
#endif
    goto error;
  }

#ifdef INET6
  /* rfc2732 compliance */
  hname = hostname;
  if (hname != nullptr && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
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
  if ((proto = getprotobyname("tcp")) == nullptr) {
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
    if ((entry = gethostbyname(hostname)) == nullptr) {
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

StreamStatus UrlStream::openHttp(const char *url, Url *pu,
                                 std::optional<Url> current,
                                 const HttpOption &option, FormList *request,
                                 TextList *extra_header, HttpRequest *hr) {
  StreamStatus status = HTST_NORMAL;
  int sock = -1;
  SSL *sslh = nullptr;
  Str *tmp = nullptr;
  if (pu->file.empty()) {
    pu->file = allocStr("/", -1);
  }
  if (request && request->method == FORM_METHOD_POST && request->body) {
    hr->method = HR_COMMAND_POST;
  }
  if (request && request->method == FORM_METHOD_HEAD) {
    hr->method = HR_COMMAND_HEAD;
  }

  {
    sock = openSocket(pu->host.c_str(), schemaNumToName(pu->schema), pu->port);
    if (sock < 0) {
      return HTST_MISSING;
    }
    if (pu->schema == SCM_HTTPS) {
      if (!(sslh = openSSLHandle(sock, pu->host.c_str(),
                                 &this->ssl_certificate))) {
        return HTST_MISSING;
      }
    }
    hr->flag |= HR_FLAG_LOCAL;
    tmp = hr->to_Str(*pu, current, extra_header);
    status = HTST_NORMAL;
  }
  if (pu->schema == SCM_HTTPS) {
    this->stream = newSSLStream(sslh, sock);
    if (sslh)
      SSL_write(sslh, tmp->ptr, tmp->length);
    else
      write(sock, tmp->ptr, tmp->length);
    if (w3m_reqlog) {
      FILE *ff = fopen(w3m_reqlog, "a");
      if (ff == nullptr)
        return status;
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
    return status;
  } else {
    write(sock, tmp->ptr, tmp->length);
    if (w3m_reqlog) {
      FILE *ff = fopen(w3m_reqlog, "a");
      if (ff == nullptr)
        return status;
      fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
      fclose(ff);
    }
    if (hr->method == HR_COMMAND_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART)
      write_from_file(sock, request->body);
  }

  this->stream = newInputStream(sock);
  return status;
}

void UrlStream::openData(Url *pu) {
  if (pu->file.empty()) {
    return;
  }

  auto p = Strnew(pu->file)->ptr;
  auto q = strchr(p, ',');
  if (q == nullptr) {
    return;
  }

  *(char *)q++ = '\0';
  auto tmp = Strnew_charp(q);
  q = strrchr(p, ';');
  if (q != nullptr && !strcmp(q, ";base64")) {
    *(char *)q = '\0';
    this->encoding = ENC_BASE64;
  } else {
    tmp = Str_url_unquote(tmp, false, false);
  }
  this->stream = newStrStream(tmp);
  this->guess_type = (*p != '\0') ? p : "text/plain";
}

StreamStatus UrlStream::openURL(const char *url, Url *pu,
                                std::optional<Url> current,
                                const HttpOption &option, FormList *request,
                                TextList *extra_header, HttpRequest *hr) {
  HttpRequest hr0;
  if (hr == nullptr)
    hr = &hr0;

  auto u = url;
  auto current_schema = parseUrlSchema(&u);
  if (!current && current_schema == SCM_MISSING && !ArgvIsURL) {
    u = file_to_url(url); /* force to local file */
  } else {
    u = url;
  }

  // retry:

  *pu = Url::parse2(u, current);
  if (pu->schema == SCM_LOCAL && pu->file.empty()) {
    if (pu->label.size()) {
      /* #hogege is not a label but a filename */
      auto tmp2 = Strnew_charp("#");
      Strcat(tmp2, pu->label);
      pu->file = tmp2->ptr;
      pu->real_file = cleanupName(file_unquote(pu->file.c_str()));
      pu->label = nullptr;
    } else {
      /* given URL must be null string */
#ifdef SOCK_DEBUG
      sock_log("given URL must be null string\n");
#endif
      return HTST_NORMAL;
    }
  }

  if (LocalhostOnly && pu->host.size() && !is_localhost(pu->host.c_str()))
    pu->host = nullptr;

  this->schema = pu->schema;
  this->url = Strnew(pu->to_Str())->ptr;
  pu->is_nocache = (option.no_cache);
  this->ext = filename_extension(pu->file.c_str(), 1);

  hr->method = HR_COMMAND_GET;
  hr->flag = {};
  hr->referer = option.referer;
  hr->request = request;

  switch (pu->schema) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    this->openLocalCgi(pu, current, option, request, extra_header, hr);
    return HTST_NORMAL;

  case SCM_HTTP:
  case SCM_HTTPS:
    return openHttp(url, pu, current, option, request, extra_header, hr);

  case SCM_DATA:
    openData(pu);
    return HTST_NORMAL;

  case SCM_UNKNOWN:
  default:
    return HTST_NORMAL;
  }

  // not reach here
  assert(false);
  return HTST_UNKNOWN;
}

static FILE *lessopen_stream(const char *path) {

  auto lessopen = getenv("LESSOPEN");
  if (lessopen == nullptr || lessopen[0] == '\0')
    return nullptr;

  if (lessopen[0] != '|') /* filename mode, not supported m(__)m */
    return nullptr;

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
          return nullptr;
        n++;
      } else
        return nullptr;
    }
  }
  if (!n)
    return nullptr;

  auto tmpf = Sprintf(lessopen, shell_quote((char *)path));
  auto fp = popen(tmpf->ptr, "r");
  if (fp == nullptr) {
    return nullptr;
  }
  auto c = getc(fp);
  if (c == EOF) {
    pclose(fp);
    return nullptr;
  }
  ungetc(c, fp);
  return fp;
}

void UrlStream::close() {
  if (ISclose(this->stream) == 0) {
    this->stream = NULL;
  }
}

void UrlStream::openFile(const char *path) {
  struct stat stbuf;

  this->guess_type = nullptr;
  if (path == nullptr || *path == '\0' || stat(path, &stbuf) == -1 ||
      NOT_REGULAR(stbuf.st_mode)) {
    this->stream = nullptr;
    return;
  }

  this->stream = openIS(path);
  if (!do_download) {
    if (use_lessopen && getenv("LESSOPEN") != nullptr) {
      FILE *fp;
      this->guess_type = guessContentType(path);
      if (this->guess_type == nullptr)
        this->guess_type = "text/plain";
      if (is_html_type(this->guess_type))
        return;
      if ((fp = lessopen_stream(path))) {
        this->close();
        this->stream = newFileStream(fp, (void (*)())pclose);
        this->guess_type = "text/plain";
        return;
      }
    }
    check_compression(path, this);
    if (this->compression != CMP_NOCOMPRESS) {
      auto ext = this->ext;
      auto t0 = uncompressed_file_type(path, &ext);
      this->guess_type = t0;
      this->ext = ext;
      this->uncompress_stream();
      return;
    }
  }
}

int UrlStream::save2tmp(const char *tmpf) const {
  FILE *ff;
  long long linelen = 0;
  // long long trbyte = 0;
  MySignalHandler prevtrap = nullptr;
  static JMP_BUF env_bak;
  int retval = 0;
  char *buf = nullptr;

  ff = fopen(tmpf, "wb");
  if (ff == nullptr) {
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
    while ((count = ISread_n(this->stream, buf, SAVE_BUF_SIZE)) > 0) {
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

int UrlStream::doFileSave(const char *defstr) {
  Str *msg;
  Str *filen;
  const char *p, *q;
  pid_t pid;
  char *lock;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == nullptr || *p == '\0') {
      // p = inputLineHist("(Download)Save file to: ", defstr, IN_FILENAME,
      //                   SaveHist);
      if (p == nullptr || *p == '\0')
        return -1;
    }
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(this->stream, p) < 0) {
      msg = Sprintf("Can't save. Load file and %s are identical.", p);
      disp_err_message(msg->ptr, false);
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    symlink(p, lock);

    flush_tty();
    pid = fork();
    if (!pid) {
      int err;
      if ((this->content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        auto tmpf = this->uncompress_stream();
        if (tmpf.size()) {
          unlink(tmpf.c_str());
        }
      }
      setup_child(false, 0, this->fileno());
      err = this->save2tmp(p);
      if (err == 0 && PreserveTimestamp && this->modtime != -1)
        setModtime(p, this->modtime);
      this->close();
      unlink(lock);
      if (err != 0)
        exit(-err);
      exit(0);
    }
    addDownloadList(pid, this->url, p, lock, 0 /*current_content_length*/);
  } else {
    q = searchKeyData();
    if (q == nullptr || *q == '\0') {
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
    if (checkSaveFile(this->stream, p) < 0) {
      printf("Can't save. Load file and %s are identical.", p);
      return -1;
    }
    if (this->content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
      auto tmpf = this->uncompress_stream();
      if (tmpf.size()) {
        unlink(tmpf.c_str());
      }
    }
    if (this->save2tmp(p) < 0) {
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && this->modtime != -1)
      setModtime(p, this->modtime);
  }
  return 0;
}

uint8_t UrlStream::getc() const { return ISgetc(this->stream); }
void UrlStream::undogetc() { ISundogetc(this->stream); }
int UrlStream::fileno() const { return ISfileno(this->stream); }
