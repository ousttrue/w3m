#include "http_stream.h"
#include "alloc.h"
#include "file/file.h"
#include "html/form.h"
#include "input/ftp.h"
#include "input/http.h"
#include "input/https.h"
#include "input/isocket.h"
#include "input/istream.h"
#include "input/localcgi.h"
#include "input/proxy.h"
#include "text/text.h"
#include "text/textlist.h"
#include <assert.h>
#include <openssl/ssl.h>
#include <openssl/types.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

bool LocalhostOnly = false;
const char *index_file = nullptr;
const char *document_root = nullptr;

static union input_stream *add_index_file(struct Url *pu,
                                          union input_stream *stream) {
  struct TextList *index_file_list = NULL;
  if (non_null(index_file))
    index_file_list = make_domain_list(index_file);
  if (index_file_list == NULL) {
    return NULL;
  }

  for (auto ti = index_file_list->first; ti; ti = ti->next) {
    const char *p =
        Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
    p = cleanupName(p);
    auto q = cleanupName(file_unquote(p));
    auto index = examineFile(q);
    if (index != NULL) {
      pu->file = p;
      pu->real_file = q;
      return index;
    }
  }

  return stream;
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

struct HttpResponse *
openHttpStream(struct HttpRequest *hr,
               // const char *url, struct Url *pu,
               // struct Url *current, const char *referer, bool no_cache,
               // struct FormList *form, struct TextList *extra_header,
               union input_stream *ouf) {
  assert(hr);
  // struct HttpRequest hr0;
  // if (!hr) {
  //   hr = &hr0;
  // }

  // auto u = url;
  // auto scheme = getURLScheme(&u);
  // if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL) {
  //   u = file_to_url(url); /* force to local file */
  // } else {
  //   u = url;
  // }
  // parseURL2(u, pu, current);

  auto res = newHttpResponse(hr);
  res->stream = ouf;

  if (hr->url.scheme == SCM_LOCAL && hr->url.file == NULL) {
    if (hr->url.label != NULL) {
      /* #hogege is not a label but a filename */
      Str tmp2 = Strnew_charp("#");
      Strcat_charp(tmp2, hr->url.label);
      hr->url.file = tmp2->ptr;
      hr->url.real_file = cleanupName(file_unquote(hr->url.file));
      hr->url.label = NULL;
    } else {
      /* given URL must be null string */
#ifdef SOCK_DEBUG
      sock_log("given URL must be null string\n");
#endif
      return res;
    }
  }

  if (LocalhostOnly && hr->url.host && !is_localhost(hr->url.host)) {
    hr->url.host = NULL;
  }

  switch (hr->url.scheme) {
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    // if (hr->form && hr->form->body)
    //   /* local CGI: POST */
    res->stream = newFileStream(localcgi_request(hr), fclose);
    // else
    //   /* lodal CGI: GET */
    //   res->stream = newFileStream(
    //       localcgi_get(pu->real_file, pu->query, referer), fclose);
    if (res->stream) {
      // uf.is_cgi = true;
      hr->url.scheme = SCM_LOCAL_CGI;
      return res;
    }

    res->stream = examineFile(hr->url.real_file);
    if (!res->stream) {
      if (dir_exist(hr->url.real_file)) {
        add_index_file(&hr->url, res->stream);
        if (!res->stream)
          return nullptr;
      } else if (document_root != NULL) {
        auto tmp = Strnew_charp(document_root);
        if (Strlastchar(tmp) != '/' && hr->url.file[0] != '/')
          Strcat_char(tmp, '/');
        Strcat_charp(tmp, hr->url.file);
        auto p = cleanupName(tmp->ptr);
        auto q = cleanupName(file_unquote(p));
        if (dir_exist(q)) {
          hr->url.file = p;
          hr->url.real_file = q;
          res->stream = add_index_file(&hr->url, res->stream);
          if (!res->stream) {
            return nullptr;
          }
        } else {
          res->stream = examineFile(q);
          if (res->stream) {
            hr->url.file = p;
            hr->url.real_file = q;
          }
        }
      }
    }
    return res;

  case SCM_FTP:
  case SCM_FTPDIR:
    if (hr->url.file == NULL)
      hr->url.file = allocStr("/", -1);
    if (non_null(FTP_proxy) && use_proxy && hr->url.host != NULL &&
        !check_no_proxy(hr->url.host)) {
      hr->flag |= HR_FLAG_PROXY;
      SocketType sock;
      if (!socketOpen(FTP_proxy_parsed.host,
                      schemeNumToName(FTP_proxy_parsed.scheme),
                      FTP_proxy_parsed.port, &sock)) {
        return res;
      }
#ifdef _WIN32
      assert(false);
#else
      stream = newInputStream(sock);
#endif
      hr->url.scheme = SCM_HTTP;
      auto tmp = HTTPrequestToStr(hr);
      socketWrite(sock, tmp->ptr, tmp->length);
    } else {
      res->stream = openFTPStream(&hr->url);
      return res;
    }
    break;

  case SCM_HTTP:
  case SCM_HTTPS: {
    if (hr->url.file == NULL)
      hr->url.file = allocStr("/", -1);
    if (hr->form && hr->form->method == FORM_METHOD_POST && hr->form->body)
      hr->command = HR_COMMAND_POST;
    if (hr->form && hr->form->method == FORM_METHOD_HEAD)
      hr->command = HR_COMMAND_HEAD;

    Str tmp = nullptr;
    SocketType sock = socketInvalid();
    SSL *sslh = NULL;
    char *ssl_certificate = nullptr;
    if (((hr->url.scheme == SCM_HTTPS) ? non_null(HTTPS_proxy)
                                       : non_null(HTTP_proxy)) &&
        use_proxy && hr->url.host != NULL && !check_no_proxy(hr->url.host)) {
      hr->flag |= HR_FLAG_PROXY;
      if (hr->url.scheme == SCM_HTTPS && res->stream_status == STREAM_CONNECT) {
        sock = ssl_socket_of(ouf);
        char *ssl_certificate;
        if (!(sslh = openSSLHandle(sock, hr->url.host, &ssl_certificate))) {
          res->stream_status = STREAM_MISSING;
          return nullptr;
        }
      } else if (hr->url.scheme == SCM_HTTPS) {
        if (!socketOpen(HTTPS_proxy_parsed.host,
                        schemeNumToName(HTTPS_proxy_parsed.scheme),
                        HTTPS_proxy_parsed.port, &sock)) {
          return nullptr;
        }
        sslh = NULL;
      } else {
        if (!socketOpen(HTTP_proxy_parsed.host,
                        schemeNumToName(HTTP_proxy_parsed.scheme),
                        HTTP_proxy_parsed.port, &sock)) {
          return nullptr;
        }
        sslh = NULL;
      }
      if (hr->url.scheme == SCM_HTTPS) {
        if (res->stream_status == STREAM_NORMAL) {
          hr->command = HR_COMMAND_CONNECT;
          tmp = HTTPrequestToStr(hr);
          res->stream_status = STREAM_CONNECT;
        } else {
          hr->flag |= HR_FLAG_LOCAL;
          tmp = HTTPrequestToStr(hr);
          res->stream_status = STREAM_NORMAL;
        }
      } else {
        tmp = HTTPrequestToStr(hr);
        res->stream_status = STREAM_NORMAL;
      }
    } else {
      if (!socketOpen(hr->url.host, schemeNumToName(hr->url.scheme),
                      hr->url.port, &sock)) {
        res->stream_status = STREAM_MISSING;
        return nullptr;
      }
      if (hr->url.scheme == SCM_HTTPS) {
        if (!(sslh = openSSLHandle(sock, hr->url.host, &ssl_certificate))) {
          res->stream_status = STREAM_MISSING;
          return nullptr;
        }
      }
      hr->flag |= HR_FLAG_LOCAL;
      tmp = HTTPrequestToStr(hr);
      res->stream_status = STREAM_NORMAL;
    }
#ifdef _WIN32
    res->stream = newWinsockStream(sock);
#else
    res->stream = newInputStream(sock);
#endif

    if (hr->url.scheme == SCM_HTTPS) {
      res->stream = newSSLStream(sslh, sock);
      ssl_set_certificate(res->stream, ssl_certificate);
      if (sslh)
        SSL_write(sslh, tmp->ptr, tmp->length);
      else
        socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          hr->form->enctype == FORM_ENCTYPE_MULTIPART) {
        if (sslh)
          SSL_write_from_file(sslh, hr->form->body);
        else
          write_from_file(sock, hr->form->body);
      }
    } else {
      socketWrite(sock, tmp->ptr, tmp->length);
      if (hr->command == HR_COMMAND_POST &&
          hr->form->enctype == FORM_ENCTYPE_MULTIPART)
        write_from_file(sock, hr->form->body);
    }
    return res;
  }

  case SCM_DATA: {
    if (hr->url.file == NULL)
      return nullptr;
    auto p = Strnew_charp(hr->url.file)->ptr;
    auto q = strchr(p, ',');
    if (q == NULL)
      return nullptr;
    *q++ = '\0';
    auto tmp = Strnew_charp(q);
    q = strrchr(p, ';');
    if (q != NULL && !strcmp(q, ";base64")) {
      *q = '\0';
      // uf.encoding = ENC_BASE64;
    } else
      tmp = Str_url_unquote(tmp, false, false);
    res->stream = newStrStream(tmp);
    // uf.guess_type = (*p != '\0') ? p : "text/plain";
    return res;
  }

  default:
    break;
  }
  return nullptr;
}
