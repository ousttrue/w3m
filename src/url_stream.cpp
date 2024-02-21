#include "url_stream.h"
#include "file_util.h"
#include "quote.h"
#include "http_session.h"
#include "rc.h"
#include "app.h"
#include "alloc.h"
#include "screen.h"
#include "istream.h"
#include "myctype.h"
#include "downloadlist.h"
#include "alloc.h"
#include "history.h"
#include "compression.h"
#include "mimetypes.h"
#include "ssl_util.h"
#include "local_cgi.h"
#include "html/form.h"
#include "etc.h"
#include "url.h"
#include "w3m.h"
#include "istream.h"
#include "http_request.h"
#include "textlist.h"
#include "linein.h"
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <string.h>

#ifdef _MSC_VER
// #include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
// #include <stdlib.h>
// #include <stdio.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

bool DecodeURL = false;
bool ArgvIsURL = true;
bool LocalhostOnly = false;
bool retryAsHttp = true;

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

bool PermitSaveToPipe = false;
bool AutoUncompress = false;
bool PreserveTimestamp = true;

// static JMP_BUF AbortLoading;
// static void KeyAbort(SIGNAL_ARG) {
//   LONGJMP(AbortLoading, 1);
//   SIGNAL_RETURN;
// }

char *index_file = nullptr;

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
#ifdef _MSC_VER
#else
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
#endif
}

void UrlStream::openLocalCgi(const std::shared_ptr<HttpRequest> &hr,
                             const HttpOption &option, FormList *request) {
  if (request && request->body) {
    /* local CGI: POST */
    this->stream =
        newFileStream(localcgi_post(hr->url.real_file.c_str(),
                                    hr->url.query.c_str(), request, option),
                      fclose);
  } else {
    /* lodal CGI: GET */
    this->stream = newFileStream(
        localcgi_get(hr->url.real_file.c_str(), hr->url.query.c_str(), option),
        fclose);
  }

  if (this->stream) {
    this->is_cgi = true;
    // this->schema = pu->schema = SCM_LOCAL_CGI;
    return;
  }

  this->openFile(hr->url.real_file.c_str());
  if (this->stream == nullptr) {
    if (dir_exist(hr->url.real_file.c_str())) {
      add_index_file(&hr->url);
      if (!this->stream) {
        return;
      }
    } else if (document_root != nullptr) {
      auto tmp = Strnew_charp(document_root);
      if (Strlastchar(tmp) != '/' && hr->url.file[0] != '/')
        Strcat_char(tmp, '/');
      Strcat(tmp, hr->url.file);
      auto p = cleanupName(tmp->ptr);
      auto q = cleanupName(file_unquote(p));
      if (dir_exist(q)) {
        hr->url.file = p;
        hr->url.real_file = (char *)q;
        add_index_file(&hr->url);
        if (!this->stream) {
          return;
        }
      } else {
        this->openFile(q);
        if (this->stream) {
          hr->url.file = p;
          hr->url.real_file = (char *)q;
        }
      }
    }
  }
}

static int openSocket(const char *const hostname, const char *remoteport_name,
                      unsigned short remoteport_num) {
  int sock = -1;
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

  App::instance().message(Sprintf("Opening socket...")->ptr, 0, 0);
  //   if (SETJMP(AbortLoading) != 0) {
  // #ifdef SOCK_DEBUG
  //     sock_log("openSocket() failed. reason: user abort\n");
  // #endif
  //     if (sock >= 0)
  //       close(sock);
  //     goto error;
  //   }
  //   TRAP_ON;
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
#ifdef _MSC_VER
        closesocket(sock);
#else
        close(sock);
#endif
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
  memset(&hostaddr, 0, sizeof(struct sockaddr_in));
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
    memcpy(&hostaddr.sin_addr, &adr, sizeof(long));
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
      memcpy(&hostaddr.sin_addr, h_addr_list[0], entry->h_length);
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

  return sock;

error:
  return -1;
}

void UrlStream::openHttp(const std::shared_ptr<HttpRequest> &hr,

                         const HttpOption &option, FormList *request) {
  hr->status = HTST_NORMAL;
  int sock = -1;
  SSL *sslh = nullptr;
  Str *tmp = nullptr;
  if (hr->url.file.empty()) {
    hr->url.file = "/";
  }
  if (request && request->method == FORM_METHOD_POST && request->body) {
    hr->method = HttpMethod::POST;
  }
  if (request && request->method == FORM_METHOD_HEAD) {
    hr->method = HttpMethod::HEAD;
  }

  {
    sock = openSocket(hr->url.host.c_str(), schemaNumToName(hr->url.schema),
                      hr->url.port);
    if (sock < 0) {
      hr->status = HTST_MISSING;
      return;
    }
    if (hr->url.schema == SCM_HTTPS) {
      if (!(sslh = openSSLHandle(sock, hr->url.host.c_str(),
                                 &this->ssl_certificate))) {
        hr->status = HTST_MISSING;
        return;
      }
    }
    hr->flag |= HR_FLAG_LOCAL;
    tmp = hr->to_Str();
    hr->status = HTST_NORMAL;
  }
  if (hr->url.schema == SCM_HTTPS) {
    this->stream = newSSLStream(sslh, sock);
    if (sslh)
      SSL_write(sslh, tmp->ptr, tmp->length);
    else {
#ifdef _MSC_VER
      send(sock, tmp->ptr, tmp->length, {});
#else
      write(sock, tmp->ptr, tmp->length);
#endif
    }
    if (w3m_reqlog) {
      FILE *ff = fopen(w3m_reqlog, "a");
      if (ff == nullptr) {
        return;
      }
      if (sslh)
        fputs("HTTPS: request via SSL\n", ff);
      else
        fputs("HTTPS: request without SSL\n", ff);
      fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
      fclose(ff);
    }
    if (hr->method == HttpMethod::POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      if (sslh)
        SSL_write_from_file(sslh, request->body);
      else
        write_from_file(sock, request->body);
    }
    return;
  } else {
#ifdef _MSC_VER
    send(sock, tmp->ptr, tmp->length, {});
#else
    write(sock, tmp->ptr, tmp->length);
#endif
    if (w3m_reqlog) {
      FILE *ff = fopen(w3m_reqlog, "a");
      if (ff == nullptr) {
        return;
      }
      fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
      fclose(ff);
    }
    if (hr->method == HttpMethod::POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      write_from_file(sock, request->body);
    }
  }

  this->stream = newInputStream(sock);
  return;
}

void UrlStream::openData(const Url &url) {
  if (url.file.empty()) {
    return;
  }

  auto p = Strnew(url.file)->ptr;
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
  this->stream = newStrStream(tmp->ptr);
  this->guess_type = (*p != '\0') ? p : "text/plain";
}

std::shared_ptr<HttpRequest> UrlStream::openURL(std::string_view path,
                                                std::optional<Url> current,
                                                const HttpOption &option,
                                                FormList *request) {
  // auto u = path;
  // auto current_schema = parseUrlSchema(&u);
  // if (!current && current_schema == SCM_MISSING && !ArgvIsURL) {
  //   u = file_to_url(url); /* force to local file */
  // } else {
  //   u = url;
  // }

  auto url = urlParse(Strnew(path)->ptr, current);
  if (url.schema == SCM_LOCAL && url.file.empty()) {
    if (url.label.size()) {
      /* #hogege is not a label but a filename */
      auto tmp2 = Strnew_charp("#");
      Strcat(tmp2, url.label);
      url.file = tmp2->ptr;
      url.real_file = cleanupName(file_unquote(url.file.c_str()));
      url.label = {};
    } else {
      /* given URL must be null string */
#ifdef SOCK_DEBUG
      sock_log("given URL must be null string\n");
#endif
      // hr->status = HTST_NORMAL;
      return std::make_shared<HttpRequest>(url, current, option, request);
    }
  }

  if (LocalhostOnly && url.host.size() &&
      !App::instance().is_localhost(url.host)) {
    url.host = {};
  }
  auto hr = std::make_shared<HttpRequest>(url, current, option, request);

  // this->schema = pu->schema;
  // this->url = Strnew(pu->to_Str())->ptr;
  // this->ext = filename_extension(pu->file.c_str(), 1);

  switch (hr->url.schema) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    this->openLocalCgi(hr, option, request);
    if (!this->stream && retryAsHttp && path[0] != '/') {
      // auto u = url;
      // auto schema = parseUrlSchema(&u);
      // if (schema == SCM_MISSING || schema == SCM_UNKNOWN) {
      //   /* retry it as "http://" */
      //   u = Strnew_m_charp("http://", url, nullptr)->ptr;
      //   return openURL(u, pu, current, option, request);
      // }
    }
    hr->status = HTST_NORMAL;
    break;

  case SCM_HTTP:
  case SCM_HTTPS:
    openHttp(hr, option, request);
    break;

  case SCM_DATA:
    openData(hr->url);
    hr->status = HTST_NORMAL;
    break;

  case SCM_UNKNOWN:
  default:
    hr->status = HTST_NORMAL;
    break;
  }

  // not reach here
  return hr;
}

static bool exists(const char *path) {
  if (path == nullptr || *path == '\0') {
    return false;
  }
  struct stat stbuf;
  if (stat(path, &stbuf) == -1) {
    return false;
  }
  if (NOT_REGULAR(stbuf.st_mode)) {
    return false;
  }
  return true;
}

void UrlStream::openFile(const char *path) {

  this->guess_type = {};
  this->stream = nullptr;
  if (!exists(path)) {
    return;
  }

  this->stream = openIS(path);
  // if (!do_download)
  {
    this->compression = CMP_NOCOMPRESS;
    if (auto d = check_compression(path)) {
      this->compression = d->type;
      this->guess_type = d->mime_type;
    }

    if (this->compression != CMP_NOCOMPRESS) {
      auto [t0, ext] = uncompressed_file_type(path);
      this->ext = ext;
      this->guess_type = t0;
      this->ext = ext;
      this->uncompress_stream();
      return;
    }
  }
}

int save2tmp(const std::shared_ptr<input_stream> &stream, const char *tmpf) {

  // long long linelen = 0;
  int retval = 0;
  char *buf = nullptr;

  auto ff = fopen(tmpf, "wb");
  if (!ff) {
    /* fclose(f); */
    return -1;
  }

  // static JMP_BUF env_bak;
  // bcopy(AbortLoading, env_bak, sizeof(JMP_BUF));
  // MySignalHandler prevtrap = nullptr;
  // if (SETJMP(AbortLoading) != 0) {
  //   goto _end;
  // }
  // TRAP_ON;
  {
    int count;
    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = stream->ISread_n(buf, SAVE_BUF_SIZE)) > 0) {
      if (static_cast<int>(fwrite(buf, 1, count, ff)) != count) {
        retval = -2;
        goto _end;
      }
      // linelen += count;
      // showProgress(&linelen, &trbyte, current_content_length);
    }
  }
_end:
  // bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
  // TRAP_OFF;
  xfree(buf);
  fclose(ff);
  // current_content_length = 0;
  return retval;
}

static int checkSaveFile(const std::shared_ptr<input_stream> &stream,
                         const char *path2) {
  int des = stream->ISfileno();
  if (des < 0)
    return 0;

  if (*path2 == '|' && PermitSaveToPipe)
    return 0;

  struct stat st1, st2;
  if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int UrlStream::doFileSave(const char *defstr) {
#ifdef _MSC_VER
  return {};
#else
  if (true/*fmInitialized*/) {
    auto p = App::instance().searchKeyData();
    if (p == nullptr || *p == '\0') {
      // p = inputLineHist("(Download)Save file to: ", defstr, IN_FILENAME,
      //                   SaveHist);
      if (p == nullptr || *p == '\0')
        return -1;
    }
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(this->stream, p) < 0) {
      auto msg = Sprintf("Can't save. Load file and %s are identical.", p);
      App::instance().disp_err_message(msg->ptr);
      return -1;
    }
    auto lock = App::instance().tmpfname(TMPF_DFL, ".lock");
    symlink(p, lock.c_str());

    // flush_tty();
    auto pid = fork();
    if (!pid) {
      int err;
      if ((this->content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        auto tmpf = this->uncompress_stream();
        if (tmpf.size()) {
          unlink(tmpf.c_str());
        }
      }
      setup_child(false, 0, this->stream->ISfileno());
      err = save2tmp(this->stream, p);
      if (err == 0 && PreserveTimestamp && this->modtime != -1)
        setModtime(p, this->modtime);
      unlink(lock.c_str());
      if (err != 0)
        exit(-err);
      exit(0);
    }
    addDownloadList(pid, this->url, p, Strnew(lock)->ptr,
                    0 /*current_content_length*/);
  } else {
    auto q = App::instance().searchKeyData();
    if (q == nullptr || *q == '\0') {
      printf("(Download)Save file to: ");
      fflush(stdout);
      auto filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    const char *p;
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
    if (save2tmp(this->stream, p) < 0) {
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && this->modtime != -1)
      setModtime(p, this->modtime);
  }
  return 0;
#endif
}

static char url_unquote_char(const char **pstr) {
  return ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2]))
              ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) |
                                   GET_MYCDIGIT((*(pstr))[-1]))
              : -1);
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
  return url_unquote_conv0(url);
}

const char *file_quote(const char *str) {
  Str *tmp = NULL;
  const char *p;
  char buf[4];

  for (p = str; *p; p++) {
    if (is_file_quote(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp) {
    return tmp->ptr;
  }
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

std::string UrlStream::uncompress_stream() {
#ifdef _MSC_VER
  return {};
#else
  if (this->stream->IStype() != IST_ENCODED) {
    this->stream = newEncodedStream(this->stream, this->encoding);
    this->encoding = ENC_7BIT;
  }

  const char *expand_cmd = GUNZIP_CMDNAME;
  const char *expand_name = GUNZIP_NAME;
  int use_d_arg = 0;
  for (auto &d : compression_decoders) {
    if (this->compression == d.type) {
      if (d.auxbin_p)
        expand_cmd = auxbinFile(d.cmd);
      else
        expand_cmd = d.cmd;
      expand_name = d.name;
      ext = d.ext;
      use_d_arg = d.use_d_arg;
      break;
    }
  }
  this->compression = CMP_NOCOMPRESS;

  std::string tmpf;
  if (this->schema != SCM_LOCAL) {
    tmpf = App::instance().tmpfname(TMPF_DFL, ext);
  }

  /* child1 -- stdout|f1=uf -> parent */
  FILE *f1;
  auto pid1 = open_pipe_rw(&f1, nullptr);
  if (pid1 < 0) {
    return {};
  }
  if (pid1 == 0) {
    /* child */
    FILE *f2 = stdin;

    /* uf -> child2 -- stdout|stdin -> child1 */
    auto pid2 = open_pipe_rw(&f2, nullptr);
    if (pid2 < 0) {
      exit(1);
    }
    if (pid2 == 0) {
      /* child2 */
      char *buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
      int count;
      FILE *f = nullptr;

      // int UrlStream::fileno() const { return this->stream->ISfileno(); }

      setup_child(true, 2, this->stream->ISfileno());
      if (tmpf.size()) {
        f = fopen(tmpf.c_str(), "wb");
      }
      while ((count = this->stream->ISread_n(buf, SAVE_BUF_SIZE)) > 0) {
        if (static_cast<int>(fwrite(buf, 1, count, stdout)) != count)
          break;
        if (f && static_cast<int>(fwrite(buf, 1, count, f)) != count)
          break;
      }
      if (f)
        fclose(f);
      xfree(buf);
      exit(0);
    }
    /* child1 */
    dup2(1, 2); /* stderr>&stdout */
    setup_child(true, -1, -1);
    if (use_d_arg)
      execlp(expand_cmd, expand_name, "-d", nullptr);
    else
      execlp(expand_cmd, expand_name, nullptr);
    exit(1);
  }
  this->stream = newFileStream(f1, fclose);
  if (tmpf.size()) {
    this->schema = SCM_LOCAL;
  }
  return tmpf;
#endif
}
