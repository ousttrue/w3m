#include "url_stream.h"
#include "opensocket.h"
#include "form.h"
#include "ioutil.h"
#include "cookie_domain.h"
#include "file_util.h"
#include "quote.h"
#include "http_session.h"
#include "rc.h"
#include "app.h"
#include "alloc.h"
#include "istream.h"
#include "myctype.h"
#include "alloc.h"
#include "compression.h"
#include "ssl_util.h"
#include "local_cgi.h"
#include "etc.h"
#include "url.h"
#include "istream.h"
#include "http_request.h"
#include "linein.h"
#include "url_decode.h"
#include "local_cgi.h"
#include "Str.h"
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#define SAVE_BUF_SIZE 1536

bool LocalhostOnly = false;

class input_stream;
struct Url;
struct Form;
struct TextList;
struct HttpRequest;
struct HttpOption;

struct UrlStream {
  const char *url = {};
  UrlScheme scheme = {};
  bool is_cgi = false;
  EncodingType encoding = ENC_7BIT;
  std::shared_ptr<input_stream> stream;
  std::string ext;
  CompressionType compression = CMP_NOCOMPRESS;
  int content_encoding = CMP_NOCOMPRESS;
  std::string guess_type;
  const char *ssl_certificate = {};
  time_t modtime = -1;

  UrlStream(UrlScheme scheme, const std::shared_ptr<input_stream> &stream = {})
      : scheme(scheme), stream(stream) {}

  std::shared_ptr<HttpRequest> openURL(const std::string &url,
                                       std::optional<Url> current,
                                       const HttpOption &option,
                                       const std::shared_ptr<Form> &request);

private:
  void openFile(const std::string &path);

  int doFileSave(const char *defstr);

  void openHttp(const std::shared_ptr<HttpRequest> &hr,
                const HttpOption &option, const std::shared_ptr<Form> &request);
  void openLocalCgi(const std::shared_ptr<HttpRequest> &hr,
                    const HttpOption &option,
                    const std::shared_ptr<Form> &request);
  void openData(const Url &pu);
  void add_index_file(Url *pu);
};

std::string index_file;

#define NOT_REGULAR(m) (((m) & S_IFMT) != S_IFREG)

/* add index_file if exists */
void UrlStream::add_index_file(Url *pu) {
  std::list<std::string> index_file_list;
  if (non_null(index_file)) {
    make_domain_list(index_file_list, index_file);
  }
  if (index_file_list.empty()) {
    this->stream = nullptr;
    return;
  }

  for (auto &ti : index_file_list) {
    std::string p = Strnew_m_charp(pu->file.c_str(), "/",
                                   ioutil::file_quote(ti).c_str(), nullptr)
                        ->ptr;
    p = cleanupName(p);
    auto q = cleanupName(ioutil::file_unquote(p));
    this->openFile(q);
    if (this->stream) {
      pu->file = p;
      pu->real_file = q;
      return;
    }
  }
}

static bool dir_exist(const std::string &path) {
  if (path.empty()) {
    return false;
  }

  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) == -1) {
    return false;
  }

  return IS_DIRECTORY(stbuf.st_mode);
}

static void write_from_file(int sock, const char *file) {
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
                             const HttpOption &option,
                             const std::shared_ptr<Form> &request) {
  if (request && request->body.size()) {
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
    // this->scheme = pu->scheme = SCM_LOCAL_CGI;
    return;
  }

  this->openFile(hr->url.real_file.c_str());
  if (this->stream == nullptr) {
    if (dir_exist(hr->url.real_file.c_str())) {
      add_index_file(&hr->url);
      if (!this->stream) {
        return;
      }
    } else if (document_root.size()) {
      auto tmp = Strnew_charp(document_root.c_str());
      if (Strlastchar(tmp) != '/' && hr->url.file[0] != '/')
        Strcat_char(tmp, '/');
      Strcat(tmp, hr->url.file);
      auto p = cleanupName(tmp->ptr);
      auto q = cleanupName(ioutil::file_unquote(p));
      if (dir_exist(q)) {
        hr->url.file = p;
        hr->url.real_file = q;
        add_index_file(&hr->url);
        if (!this->stream) {
          return;
        }
      } else {
        this->openFile(q);
        if (this->stream) {
          hr->url.file = p;
          hr->url.real_file = q;
        }
      }
    }
  }
}

void UrlStream::openHttp(const std::shared_ptr<HttpRequest> &hr,

                         const HttpOption &option,
                         const std::shared_ptr<Form> &request) {
  hr->status = HTST_NORMAL;
  int sock = -1;
  if (hr->url.file.empty()) {
    hr->url.file = "/";
  }
  if (request && request->method == FORM_METHOD_POST && request->body.size()) {
    hr->method = HttpMethod::POST;
  }
  if (request && request->method == FORM_METHOD_HEAD) {
    hr->method = HttpMethod::HEAD;
  }

  if (auto sock = openSocket(hr->url.host, schemeNumToName(hr->url.scheme),
                             hr->url.port)) {

    SslConnection sslh = {};
    if (hr->url.scheme == SCM_HTTPS) {
      sslh = openSSLHandle(*sock, hr->url.host.c_str());
      if (!sslh.handle) {
        hr->status = HTST_MISSING;
        return;
      }
    }
    hr->flag |= HR_FLAG_LOCAL;
    std::string tmp;
    tmp = hr->to_Str(w3m_version);
    hr->status = HTST_NORMAL;

    if (hr->url.scheme == SCM_HTTPS) {
      this->stream = newSSLStream(sslh.handle, *sock);
      if (sslh.handle)
        SSL_write(sslh.handle, tmp.c_str(), tmp.size());
      else {
#ifdef _MSC_VER
        send(*sock, tmp.c_str(), tmp.size(), {});
#else
        write(*sock, tmp.c_str(), tmp.size());
#endif
      }
      if (w3m_reqlog) {
        FILE *ff = fopen(w3m_reqlog, "a");
        if (ff == nullptr) {
          return;
        }
        if (sslh.handle)
          fputs("HTTPS: request via SSL\n", ff);
        else
          fputs("HTTPS: request without SSL\n", ff);
        fwrite(tmp.c_str(), 1, tmp.size(), ff);
        fclose(ff);
      }
      if (hr->method == HttpMethod::POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        if (sslh.handle)
          SSL_write_from_file(sslh.handle, request->body.c_str());
        else
          write_from_file(*sock, request->body.c_str());
      }
      return;
    } else {
#ifdef _MSC_VER
      send(*sock, tmp.c_str(), tmp.size(), {});
#else
      write(*sock, tmp.c_str(), tmp.size());
#endif
      if (w3m_reqlog) {
        FILE *ff = fopen(w3m_reqlog, "a");
        if (ff == nullptr) {
          return;
        }
        fwrite(tmp.c_str(), 1, tmp.size(), ff);
        fclose(ff);
      }
      if (hr->method == HttpMethod::POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        write_from_file(*sock, request->body.c_str());
      }
    }

    this->stream = newInputStream(*sock);

  } else {
    hr->status = HTST_MISSING;
  }
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
  std::string tmp;
  q = strrchr(p, ';');
  if (q != nullptr && !strcmp(q, ";base64")) {
    *(char *)q = '\0';
    this->encoding = ENC_BASE64;
  } else {
    tmp = Str_url_unquote(tmp, false, false);
  }
  this->stream = newStrStream(tmp.c_str());
  this->guess_type = (*p != '\0') ? p : "text/plain";
}

std::shared_ptr<HttpRequest>
UrlStream::openURL(const std::string &path, std::optional<Url> current,
                   const HttpOption &option,
                   const std::shared_ptr<Form> &request) {
  // auto u = path;
  // auto current_schema = parseUrlSchema(&u);
  // if (!current && current_schema == SCM_MISSING && !ArgvIsURL) {
  //   u = file_to_url(url); /* force to local file */
  // } else {
  //   u = url;
  // }

  Url url(path, current);
  if (url.scheme == SCM_LOCAL && url.file.empty()) {
    if (url.label.size()) {
      /* #hogege is not a label but a filename */
      auto tmp2 = Strnew_charp("#");
      Strcat(tmp2, url.label);
      url.file = tmp2->ptr;
      url.real_file = cleanupName(ioutil::file_unquote(url.file));
      url.label = {};
    } else {
      /* given URL must be null string */
      // #ifdef SOCK_DEBUG
      //       sock_log("given URL must be null string\n");
      // #endif
      // hr->status = HTST_NORMAL;
      return std::make_shared<HttpRequest>(url, current, option, request);
    }
  }

  if (LocalhostOnly && url.host.size() && !ioutil::is_localhost(url.host)) {
    url.host = {};
  }
  auto hr = std::make_shared<HttpRequest>(url, current, option, request);

  // this->scheme = pu->scheme;
  // this->url = Strnew(pu->to_Str())->ptr;
  // this->ext = filename_extension(pu->file.c_str(), 1);

  switch (hr->url.scheme) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    this->openLocalCgi(hr, option, request);
    if (!this->stream && path[0] != '/') {
      // auto u = url;
      // auto scheme = parseUrlSchema(&u);
      // if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
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

static bool exists(const std::string &path) {
  if (path.empty()) {
    return false;
  }

  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) == -1) {
    return false;
  }

  if (NOT_REGULAR(stbuf.st_mode)) {
    return false;
  }

  return true;
}

void UrlStream::openFile(const std::string &path) {

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
  if (true /*fmInitialized*/) {
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
    auto lock = TmpFile::instance().tmpfname(TMPF_DFL, ".lock");
    symlink(p, lock.c_str());

    // flush_tty();
    auto pid = fork();
    if (!pid) {
      int err;
      // if ((this->content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
      //   auto tmpf = this->uncompress_stream();
      //   if (tmpf.size()) {
      //     unlink(tmpf.c_str());
      //   }
      // }
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
    p = Strnew(expandPath(q))->ptr;
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(this->stream, p) < 0) {
      printf("Can't save. Load file and %s are identical.", p);
      return -1;
    }
    // if (this->content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
    //   auto tmpf = this->uncompress_stream();
    //   if (tmpf.size()) {
    //     unlink(tmpf.c_str());
    //   }
    // }
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

std::tuple<std::shared_ptr<HttpRequest>, std::shared_ptr<input_stream>>
openURL(const std::string &url, std::optional<Url> current,
        const HttpOption &option, const std::shared_ptr<Form> &form) {
  UrlStream f(SCM_UNKNOWN);
  auto res = f.openURL(url, current, option, form);
  return {res, f.stream};
}
