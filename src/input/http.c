#include "http.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/message.h"
#include "core.h"
#include "file/file.h"
#include "func.h"
#include "input/http_auth.h"
#include "input/http_cookie.h"
#include "input/http_stream.h"
#include "input/istream.h"
#include "input/mimehead.h"
#include "input/proxy.h"
#include "input/url.h"
#include "siteconf.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/datetime.h"
#include "text/myctype.h"
#include "text/text.h"
#include "version.h"
#include <stdint.h>
#include <string.h>

int FollowRedirection = 10;
bool retryAsHttp = true;
bool override_user_agent = false;
const char *UserAgent = nullptr;
const char *AcceptLang = nullptr;
const char *AcceptEncoding = nullptr;
const char *AcceptMedia = nullptr;
bool NoCache = false;
bool NoSendReferer = false;
bool CrossOriginReferer = true;
bool override_content_type = false;

static bool same_url_p(struct Url *pu1, struct Url *pu2) {
  return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
          (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1) &&
          (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

static int checkRedirection(struct Url *pu) {
  static struct Url *puv = NULL;
  static int nredir = 0;
  static int nredir_size = 0;

  if (pu == NULL) {
    nredir = 0;
    nredir_size = 0;
    puv = NULL;
    return true;
  }

  if (nredir >= FollowRedirection) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, parsedURL2Str(pu)->ptr);
    message_push(tmp->ptr);
    return false;
  } else if (nredir_size > 0 &&
             (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) ||
              (!(nredir % 2) &&
               same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    auto tmp =
        Sprintf("Redirection loop detected (%s)", parsedURL2Str(pu)->ptr);
    message_push(tmp->ptr);
    return false;
  }

  if (!puv) {
    nredir_size = FollowRedirection / 2 + 1;
    puv = New_N(struct Url, nredir_size);
    memset(puv, 0, sizeof(struct Url) * nredir_size);
  }
  copyParsedURL(&puv[nredir % nredir_size], pu);
  nredir++;
  return true;
}

void clearRedirection() { checkRedirection(NULL); }

struct HttpRequest *newHttpRequest(struct Url url, struct FormList *form,
                                   const char *referer, bool no_cache,
                                   struct TextList *extra_header) {
  auto hr = New(struct HttpRequest);
  hr->url = url;
  hr->form = form;
  hr->command = HR_COMMAND_GET;
  hr->flag = 0;
  hr->referer = referer;
  hr->no_cache = no_cache;
  hr->form = form;
  hr->extra_header = extra_header;
  return hr;
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
  return nullptr;
}

Str HTTPrequestURI(struct HttpRequest *hr) {
  Str tmp = Strnew();
  if (hr->command == HR_COMMAND_CONNECT) {
    Strcat_charp(tmp, hr->url.host);
    Strcat(tmp, Sprintf(":%d", hr->url.port));
  } else if (hr->flag & HR_FLAG_LOCAL) {
    Strcat_charp(tmp, hr->url.file);
    if (hr->url.query) {
      Strcat_char(tmp, '?');
      Strcat_charp(tmp, hr->url.query);
    }
  } else
    Strcat(tmp, _parsedURL2Str(&hr->url, true, true, false));
  return tmp;
}

// {name}={value} ; {name} ;
bool httpMatchattr(const char *p, const char *attr, int len, Str *value) {
  if (strncasecmp(p, attr, len) == 0) {
    p += len;
    SKIP_BLANKS(p);
    if (value) {
      *value = Strnew();
      if (*p == '=') {
        p++;
        SKIP_BLANKS(p);
        bool quoted = 0;
        const char *q = NULL;
        while (!IS_ENDL(*p) && (quoted || *p != ';')) {
          if (!IS_SPACE(*p))
            q = p;
          if (*p == '"')
            quoted = (quoted) ? 0 : 1;
          else
            Strcat_char(*value, *p);
          p++;
        }
        if (q)
          Strshrink(*value, p - q - 1);
      }
      return 1;
    } else {
      if (IS_ENDT(*p)) {
        return 1;
      }
    }
  }
  return 0;
}

static Str parsedURL2RefererOriginStr(struct Url *pu) {
  auto f = pu->file;
  auto q = pu->query;
  pu->file = NULL;
  pu->query = NULL;
  auto s = _parsedURL2Str(pu, false, false, false);
  pu->file = f;
  pu->query = q;
  return s;
}

// if (hr->referer == NO_REFERER)
// else
// Strcat_charp(tmp, otherinfo(pu, NULL, NULL, no_cache));
// Strcat_charp(tmp, otherinfo(pu, current, hr->referer, no_cache));
static char *
otherinfo(struct HttpRequest *hr
          // struct Url *target, struct Url *current,
          //                      const char *referer, bool is_nocache
) {
  Str s = Strnew();
  const int *no_referer_ptr;
  int no_referer;
  const char *url_user_agent = query_SCONF_USER_AGENT(&hr->url);

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

  if (hr->url.host) {
    Strcat_charp(s, "Host: ");
    Strcat_charp(s, hr->url.host);
    if (hr->url.port != DefaultPort[hr->url.scheme])
      Strcat(s, Sprintf(":%d", hr->url.port));
    Strcat_charp(s, "\r\n");
  }
  if (hr->no_cache || NoCache) {
    Strcat_charp(s, "Pragma: no-cache\r\n");
    Strcat_charp(s, "Cache-control: no-cache\r\n");
  }

  auto current = hr->referer == NO_REFERER ? nullptr : hr->current;
  auto referer = hr->referer == NO_REFERER ? nullptr : hr->referer;
  no_referer = NoSendReferer;
  no_referer_ptr = query_SCONF_NO_REFERER_FROM(current);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  no_referer_ptr = query_SCONF_NO_REFERER_TO(&hr->url);
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  if (!no_referer) {
    int cross_origin = false;
    if (CrossOriginReferer && current && current->host &&
        (!hr->url.host || strcasecmp(current->host, hr->url.host) != 0 ||
         current->port != hr->url.port || current->scheme != hr->url.scheme))
      cross_origin = true;
    if (current && current->scheme == SCM_HTTPS &&
        hr->url.scheme != SCM_HTTPS) {
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

Str HTTPrequestToStr(struct HttpRequest *hr) {
  struct TextListItem *i;
  Str cookie;
  auto tmp = HTTPrequestMethod(hr);
  Strcat_charp(tmp, " ");
  Strcat_charp(tmp, HTTPrequestURI(hr)->ptr);
  Strcat_charp(tmp, " HTTP/1.0\r\n");
  Strcat_charp(tmp, otherinfo(hr));
  if (hr->extra_header)
    for (i = hr->extra_header->first; i != NULL; i = i->next) {
      if (strncasecmp(i->ptr, "Authorization:", sizeof("Authorization:") - 1) ==
          0) {
        if (hr->command == HR_COMMAND_CONNECT)
          continue;
      }
      if (strncasecmp(i->ptr, "Proxy-Authorization:",
                      sizeof("Proxy-Authorization:") - 1) == 0) {
        if (hr->url.scheme == SCM_HTTPS && hr->command != HR_COMMAND_CONNECT)
          continue;
      }
      Strcat_charp(tmp, i->ptr);
    }

  if (hr->command != HR_COMMAND_CONNECT && use_cookie &&
      (cookie = find_cookie(&hr->url))) {
    Strcat_charp(tmp, "Cookie: ");
    Strcat(tmp, cookie);
    Strcat_charp(tmp, "\r\n");
    /* [DRAFT 12] s. 10.1 */
    if (cookie->ptr[0] != '$')
      Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
  }
  if (hr->command == HR_COMMAND_POST) {
    if (hr->form->enctype == FORM_ENCTYPE_MULTIPART) {
      Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
      Strcat_charp(tmp, hr->form->boundary);
      Strcat_charp(tmp, "\r\n");
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->form->length));
      Strcat_charp(tmp, "\r\n");
    } else {
      if (!override_content_type) {
        Strcat_charp(tmp,
                     "Content-Type: application/x-www-form-urlencoded\r\n");
      }
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", hr->form->length));
      if (header_string)
        Strcat(tmp, header_string);
      Strcat_charp(tmp, "\r\n");
      Strcat_charp_n(tmp, hr->form->body, hr->form->length);
      Strcat_charp(tmp, "\r\n");
    }
  } else {
    if (header_string)
      Strcat(tmp, header_string);
    Strcat_charp(tmp, "\r\n");
  }
  return tmp;
}

struct HttpResponse *sendHttpRequest(struct HttpRequest *req,
                                     union input_stream *of,
                                     bool add_auth_cookie_flag, Str realm,
                                     Str uname, Str pwd) {
  auto sc_redirect = query_SCONF_SUBSTITUTE_URL(&req->url);
  if (sc_redirect && *sc_redirect && checkRedirection(&req->url)) {
    struct Url url = parseURL2(sc_redirect, &req->url);
    add_auth_cookie_flag = 0;
    req = newHttpRequest(url, nullptr, req->referer, req->no_cache,
                         req->extra_header);
    return sendHttpRequest(req, of, add_auth_cookie_flag, realm, uname, pwd);
  }

  auto http_response = openHttpStream(req, of);
  if ((!http_response || http_response->stream == NULL) && retryAsHttp &&
      req->url.file[0] != '/') {
    if (req->url.scheme == SCM_MISSING || req->url.scheme == SCM_UNKNOWN) {
      // retry it as "http://"
      auto u = Strnew_m_charp("http://", req->url.file, NULL)->ptr;
      struct Url url = parseURL2(u, req->current);
      req = newHttpRequest(url, req->form, req->referer, req->no_cache,
                           req->extra_header);
      http_response = openHttpStream(req, of);
    }
  }

  if (http_response && http_response->stream_status == STREAM_MISSING) {
    ISclose(http_response->stream);
    return NULL;
  }

  // succeeded
  if (header_string) {
    header_string = NULL;
  }

  if (req->url.scheme == SCM_HTTP || req->url.scheme == SCM_HTTPS ||
      (((req->url.scheme == SCM_FTP && non_null(FTP_proxy))) && use_proxy &&
       !check_no_proxy(req->url.host))) {

    term_message(
        Sprintf("%s contacted. Waiting for reply...", req->url.host)->ptr);
    httpReadResponse(http_response);

    const char *p = httpGetHeader(http_response, "Location:");
    if (((http_response->http_status_code >= 301 &&
          http_response->http_status_code <= 303) ||
         http_response->http_status_code == 307) &&
        p && checkRedirection(&req->url)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      auto tpath = url_quote(p);
      // current = New(struct Url);
      // copyParsedURL(current, &pu);
      struct Url url = parseURL2(tpath, &req->url);
      // form = NULL;
      req = newHttpRequest(url, nullptr, req->referer, req->no_cache,
                           req->extra_header);
      ISclose(http_response->stream);
      // t_buf->bufferprop |= BP_REDIRECTED;
      return sendHttpRequest(req, nullptr, add_auth_cookie_flag, realm, uname,
                             pwd);
    }

    if (add_auth_cookie_flag && realm && uname && pwd) {
      /* If authorization is required and passed */
      add_auth_user_passwd(&req->url, qstr_unquote(realm)->ptr, uname, pwd, 0);
      add_auth_cookie_flag = 0;
    }

    p = httpGetHeader(http_response, "WWW-Authenticate:");
    if (p && http_response->http_status_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, http_response, "WWW-Authenticate:") !=
              NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        getAuthCookie(&hauth, "Authorization:", req, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          return http_response;
        }
        ISclose(http_response->stream);
        add_auth_cookie_flag = 1;
        return sendHttpRequest(req, nullptr, add_auth_cookie_flag, realm, uname,
                               pwd);
      }
    }

    p = httpGetHeader(http_response, "Proxy-Authenticate:");
    if (p && http_response->http_status_code == 407) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, http_response, "Proxy-Authenticate:") &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auto auth_pu = schemeToProxy(req->url.scheme);
        getAuthCookie(&hauth, "Proxy-Authorization:", req, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          return http_response;
        }

        ISclose(http_response->stream);
        add_auth_cookie_flag = 1;
        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        return sendHttpRequest(req, nullptr, add_auth_cookie_flag, realm, uname,
                               pwd);
      }
    }

    /* XXX: RFC2617 3.2.3 Authentication-Info: ? */
    if (http_response->stream_status == STREAM_CONNECT) {
      return sendHttpRequest(req, http_response->stream, add_auth_cookie_flag,
                             realm, uname, pwd);
    }

    // f.modtime = mymktime(httpGetHeader(t_buf->http_response,
    // "Last-Modified:"));
  } else if (req->url.scheme == SCM_FTP) {
    // f.compression = check_compression(path, &f.guess_type);
    // if (f.compression != CMP_NOCOMPRESS) {
    //   auto t1 = uncompressed_file_type(pu.file, NULL);
    //   real_type = f.guess_type;
    //   if (t1)
    //     t = t1;
    //   else
    //     t = real_type;
    // } else
  } else if (req->url.scheme == SCM_DATA) {
    // t = f.guess_type;
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  // f.guess_type = t;

  return http_response;
}

/* This array should be somewhere else */
/* FIXME: gettextize? */
const char *violations[COO_EMAX] = {
    "internal error",          "tail match failed",
    "wrong number of dots",    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1", "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",   "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

static void httpReadHeader(struct HttpResponse *res, union input_stream *stream,
                           const char *key, const char *p) {
  if (!strcasecmp(key, "content-type")) {
    // text/html; charset=Shift_JIS
    auto semi = strchr(p, ';');
    if (semi) {
      // check charse
      semi[0] = 0;
      auto q = semi + 1;
      SKIP_BLANKS(q);
      auto charset = Strnew();
      if (httpMatchattr(q, "charset", 7, &charset)) {
        if (!strcasecmp(charset->ptr, "shift_jis")) {
          res->content_charset = CHARSET_SJIS;
        }
      }
    }
    if (!strncasecmp(p, "text/html", 9)) {
      res->content_type = CONTENTTYPE_TextHTml;
    }
  } else if (!strcasecmp(key, "content-length")) {
    res->content_length = atoi(p);
  }
  // else if (!strcasecmp(key, "content-transfer-encoding")) {
  //   while (IS_SPACE(*p))
  //     p++;
  //   if (!strncasecmp(p, "base64", 6))
  //     uf->encoding = ENC_BASE64;
  //   else if (!strncasecmp(p, "quoted-printable", 16))
  //     uf->encoding = ENC_QUOTE;
  //   else if (!strncasecmp(p, "uuencode", 8) ||
  //            !strncasecmp(p, "x-uuencode", 10))
  //     uf->encoding = ENC_UUENCODE;
  //   else
  //     uf->encoding = ENC_7BIT;
  // }
  else if (!strcasecmp(key, "content-encoding")) {
    while (IS_SPACE(*p)) {
      p++;
    }
    // uf->compression = compressionFromEncoding(p);
    // uf->content_encoding = uf->compression;
  } else if (use_cookie && accept_cookie &&
             check_cookie_accept_domain(res->request->url.host) &&
             (!strcasecmp(key, "Set-Cookie") ||
              !strcasecmp(key, "Set-Cookie2"))) {
    Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
        comment = NULL, commentURL = NULL, port = NULL, tmp2;
    int version, quoted, flag = 0;
    time_t expires = (time_t)-1;

    const char *q = NULL;
    if (key[10] == '2') {
      version = 1;
    } else {
      version = 0;
    }
    SKIP_BLANKS(p);
    while (*p != '=' && !IS_ENDT(*p))
      Strcat_char(name, *(p++));
    Strremovetrailingspaces(name);
    if (*p == '=') {
      p++;
      SKIP_BLANKS(p);
      quoted = 0;
      while (!IS_ENDL(*p) && (quoted || *p != ';')) {
        if (!IS_SPACE(*p))
          q = p;
        if (*p == '"')
          quoted = (quoted) ? 0 : 1;
        Strcat_char(value, *(p++));
      }
      if (q)
        Strshrink(value, p - q - 1);
    }
    while (*p == ';') {
      p++;
      SKIP_BLANKS(p);
      if (httpMatchattr(p, "expires", 7, &tmp2)) {
        /* version 0 */
        expires = mymktime(tmp2->ptr);
      } else if (httpMatchattr(p, "max-age", 7, &tmp2)) {
        /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2
         */
        expires = time(NULL) + atol(tmp2->ptr);
      } else if (httpMatchattr(p, "domain", 6, &tmp2)) {
        domain = tmp2;
      } else if (httpMatchattr(p, "path", 4, &tmp2)) {
        path = tmp2;
      } else if (httpMatchattr(p, "secure", 6, NULL)) {
        flag |= COO_SECURE;
      } else if (httpMatchattr(p, "comment", 7, &tmp2)) {
        comment = tmp2;
      } else if (httpMatchattr(p, "version", 7, &tmp2)) {
        version = atoi(tmp2->ptr);
      } else if (httpMatchattr(p, "port", 4, &tmp2)) {
        /* version 1, Set-Cookie2 */
        port = tmp2;
      } else if (httpMatchattr(p, "commentURL", 10, &tmp2)) {
        /* version 1, Set-Cookie2 */
        commentURL = tmp2;
      } else if (httpMatchattr(p, "discard", 7, NULL)) {
        /* version 1, Set-Cookie2 */
        flag |= COO_DISCARD;
      }
      quoted = 0;
      while (!IS_ENDL(*p) && (quoted || *p != ';')) {
        if (*p == '"')
          quoted = (quoted) ? 0 : 1;
        p++;
      }
    }
    if (name->length > 0) {
      int err;
      if (show_cookie) {
        if (flag & COO_SECURE)
          message_push("Received a secured cookie");
        else
          message_push(
              Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr);
      }
      err = add_cookie(&res->request->url, name, value, expires, domain, path,
                       flag, comment, version, port, commentURL);
      if (err) {
        const char *ans =
            (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT) ? "y" : NULL;
        if ((err & COO_OVERRIDE_OK) &&
            accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
          Str msg = Sprintf(
              "Accept bad cookie from %s for %s?", res->request->url.host,
              ((domain && domain->ptr) ? domain->ptr : "<localdomain>"));
          if (msg->length > COLS - 10)
            Strshrink(msg, msg->length - (COLS - 10));
          Strcat_charp(msg, " (y/n)");
          ans = term_inputAnswer(msg->ptr);
        }
        if (ans == NULL || TOLOWER(*ans) != 'y' ||
            (err = add_cookie(&res->request->url, name, value, expires, domain,
                              path, flag | COO_OVERRIDE, comment, version, port,
                              commentURL))) {
          err = (err & ~COO_OVERRIDE_OK) - 1;

          const char *emsg;
          if (err >= 0 && err < COO_EMAX)
            emsg = Sprintf("This cookie was rejected "
                           "to prevent security violation. [%s]",
                           violations[err])
                       ->ptr;
          else
            emsg = "This cookie was rejected to prevent security violation.";
          message_push(emsg);
          if (show_cookie) {
            message_push(emsg);
          }
        } else if (show_cookie) {
          message_push(
              Sprintf("Accepting invalid cookie: %s=%s", name->ptr, value->ptr)
                  ->ptr);
        }
      }
    }
  } else if (!strcasecmp(key, "w3m-control") &&
             res->request->url.scheme == SCM_LOCAL_CGI) {
    Str funcname = Strnew();
    SKIP_BLANKS(p);
    while (*p && !IS_SPACE(*p))
      Strcat_char(funcname, *(p++));
    SKIP_BLANKS(p);
    int f = getFuncList(funcname->ptr);
    if (f >= 0) {
      auto tmp = Strnew_charp(p);
      Strchop(tmp);
      pushEvent(f, tmp->ptr);
    }
  }
}

struct HttpResponse *newHttpResponse(struct HttpRequest *req) {
  auto res = New(struct HttpResponse);
  res->request = req;
  res->http_status_code = -1;
  res->content_type = CONTENTTYPE_TextPlane;
  res->content_charset = CHARSET_UTF8;
  res->content_length = 0;
  res->document_header = newTextList();
  return res;
}

void httpReadResponse(struct HttpResponse *res) {
  if (res->request->url.scheme == SCM_HTTP ||
      res->request->url.scheme == SCM_HTTPS) {
    // HTTP/1.1 404 Not Found
    auto line = StrmyISgets(res->stream);
    auto p = line->ptr;
    // http version
    while (*p && !IS_SPACE(*p))
      p++;
    while (*p && IS_SPACE(*p))
      p++;
    // status code
    res->http_status_code = atoi(p);
    // term_message(line->ptr);
  } else {
    // not http
    res->http_status_code = 0;
  }

  Str lineBuf2 = nullptr;
  const char *q;
  while (true) {
    auto line = StrmyISgets(res->stream);
    if (line->length == 0) {
      break;
    }
    cleanup_line(line, HEADER_MODE);
    if (line->ptr[0] == '\n' || line->ptr[0] == '\r' || line->ptr[0] == '\0') {
      if (!lineBuf2)
        /* there is no header */
        break;
      /* last header */
    }

    if (lineBuf2) {
      Strcat(lineBuf2, line);
    } else {
      lineBuf2 = line;
    }
    char c = ISgetc(res->stream);
    ISundogetc(res->stream);
    if (c == ' ' || c == '\t') {
      /* header line is continued */
      continue;
    }

    lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
    lineBuf2 = convertLine(lineBuf2, RAW_MODE);
    /* separated with line and stored */
    auto tmp = Strnew_size(lineBuf2->length);
    for (const char *p = lineBuf2->ptr; *p; p = q) {
      for (q = p; *q && *q != '\r' && *q != '\n'; q++)
        ;
      Lineprop *propBuffer;
      lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
      Strcat(tmp, lineBuf2);
      for (; *q && (*q == '\r' || *q == '\n'); q++)
        ;
    }
    lineBuf2 = tmp;

    auto colon = strchr(lineBuf2->ptr, ':');
    if (colon) {
      pushText(res->document_header, Strdup(lineBuf2)->ptr);

      colon[0] = 0;

      auto value = colon + 1;
      SKIP_BLANKS(value);

      httpReadHeader(res, res->stream, lineBuf2->ptr, value);
    }

    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
}

const char *httpGetHeader(struct HttpResponse *res, const char *field) {
  if (res == NULL || field == NULL || res->document_header == NULL)
    return NULL;

  int len = strlen(field);
  for (auto i = res->document_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, field, len)) {
      const char *p = i->ptr + len;
      return remove_space(p);
    }
  }
  return NULL;
}

const char *httpGetContentType(struct HttpResponse *buf) {
  const char *p = httpGetHeader(buf, "Content-Type:");
  if (p == NULL)
    return NULL;

  // text/html; charset=Shift_JIS
  auto r = Strnew();
  while (*p && *p != ';' && !IS_SPACE(*p))
    Strcat_char(r, *p++);
  return r->ptr;
}

#define DEF_SAVE_FILE "index.html"

char *guess_filename(const char *file) {
  char *p = NULL;
  if (file != NULL)
    p = allocStr(mybasename(file), -1);
  if (p == NULL || *p == '\0')
    return DEF_SAVE_FILE;

  auto s = p;
  if (*p == '#')
    p++;
  while (*p != '\0') {
    if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
      *p = '\0';
      break;
    }
    p++;
  }
  return s;
}

const char *guess_save_name(struct HttpResponse *http_response,
                            const char *path) {
  Str name = NULL;
  const char *p, *q;
  if ((p = httpGetHeader(http_response, "Content-Disposition:")) != NULL &&
      (q = strcasestr(p, "filename")) != NULL &&
      (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
      httpMatchattr(q, "filename", 8, &name))
    path = name->ptr;
  else if ((p = httpGetHeader(http_response, "Content-Type:")) != NULL &&
           (q = strcasestr(p, "name")) != NULL &&
           (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
           httpMatchattr(q, "name", 4, &name))
    path = name->ptr;
  return guess_filename(path);
}
