#include "http_request.h"
#include "quote.h"
#include "w3m.h"
#include "Str.h"
#include "url.h"
#include "textlist.h"
#include "rc.h"
#include "cookie.h"
#include "html/form.h"
#include "http_auth.h"
#include "auth_pass.h"
#include "myctype.h"
#include <string.h>
#ifdef _MSC_VER
#include <io.h>
#endif

bool override_user_agent = false;
char *UserAgent = nullptr;
const char *AcceptLang = nullptr;
char *AcceptEncoding = nullptr;
char *AcceptMedia = nullptr;
bool NoCache = false;
bool NoSendReferer = false;
bool CrossOriginReferer = true;
bool override_content_type = false;
Str *header_string = nullptr;

Str *HttpRequest::getRequestURI() const {
  Str *tmp = Strnew();
  if (this->method == HttpMethod::CONNECT) {
    Strcat(tmp, url.host);
    Strcat(tmp, Sprintf(":%d", url.port));
  } else if (this->flag & HR_FLAG_LOCAL) {
    Strcat(tmp, url.file);
    if (url.query.size()) {
      Strcat_char(tmp, '?');
      Strcat(tmp, url.query);
    }
  } else {
    Strcat(tmp, url.to_Str(true, true, false));
  }
  return tmp;
}

static char *otherinfo(const Url &target, std::optional<Url> current,
                       const HttpOption &option) {
  Str *s = Strnew();
  const int *no_referer_ptr;
  int no_referer;
  const char *url_user_agent = nullptr;

  if (!override_user_agent) {
    Strcat_charp(s, "User-Agent: ");
    if (url_user_agent)
      Strcat_charp(s, url_user_agent);
    else if (UserAgent == nullptr || *UserAgent == '\0')
      Strcat_charp(s, w3m_version);
    else
      Strcat_charp(s, UserAgent);
    Strcat_charp(s, "\r\n");
  }

  Strcat_m_charp(s, "Accept: ", AcceptMedia, "\r\n", nullptr);
  Strcat_m_charp(s, "Accept-Encoding: ", AcceptEncoding, "\r\n", nullptr);
  Strcat_m_charp(s, "Accept-Language: ", AcceptLang, "\r\n", nullptr);

  if (target.host.size()) {
    Strcat_charp(s, "Host: ");
    Strcat(s, target.host);
    if (target.port != getDefaultPort(target.schema)) {
      Strcat(s, Sprintf(":%d", target.port));
    }
    Strcat_charp(s, "\r\n");
  }
  if (option.no_cache || NoCache) {
    Strcat_charp(s, "Pragma: no-cache\r\n");
    Strcat_charp(s, "Cache-control: no-cache\r\n");
  }
  no_referer = NoSendReferer;
  no_referer_ptr = nullptr;
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  no_referer_ptr = nullptr;
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  if (!no_referer) {
    int cross_origin = false;
    if (CrossOriginReferer && current && current->host.size() &&
        (target.host.empty() || current->host != target.host ||
         current->port != target.port || current->schema != target.schema)) {
      cross_origin = true;
    }
    if (current && current->schema == SCM_HTTPS && target.schema != SCM_HTTPS) {
      /* Don't send Referer: if https:// -> http:// */
    } else if (option.referer.empty() && current &&
               current->schema != SCM_LOCAL &&
               current->schema != SCM_LOCAL_CGI &&
               current->schema != SCM_DATA) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, current->RefererOriginStr());
      else
        Strcat(s, current->to_RefererStr());
      Strcat_charp(s, "\r\n");
    } else if (option.referer.size() && !option.no_referer) {
      Strcat_charp(s, "Referer: ");
      if (cross_origin)
        Strcat(s, current->RefererOriginStr());
      else
        Strcat(s, option.referer);
      Strcat_charp(s, "\r\n");
    }
  }
  return s->ptr;
}

Str *HttpRequest::to_Str() const {

  auto tmp = Strnew(to_str(this->method));
  Strcat_charp(tmp, " ");
  Strcat_charp(tmp, this->getRequestURI()->ptr);
  Strcat_charp(tmp, " HTTP/1.0\r\n");
  if (this->option.no_referer) {
    Strcat_charp(tmp, otherinfo(url, {}, this->option));
  } else {
    Strcat_charp(tmp, otherinfo(url, current, this->option));
  }
  if (extra_headers) {
    for (auto i = extra_headers->first; i != nullptr; i = i->next) {
      if (strncasecmp(i->ptr, "Authorization:", sizeof("Authorization:") - 1) ==
          0) {
        if (this->method == HttpMethod::CONNECT)
          continue;
      }
      if (strncasecmp(i->ptr, "Proxy-Authorization:",
                      sizeof("Proxy-Authorization:") - 1) == 0) {
        if (url.schema == SCM_HTTPS && this->method != HttpMethod::CONNECT)
          continue;
      }
      Strcat_charp(tmp, i->ptr);
    }
  }

  Str *cookie = {};
  if (this->method != HttpMethod::CONNECT && use_cookie &&
      (cookie = find_cookie(url))) {
    Strcat_charp(tmp, "Cookie: ");
    Strcat(tmp, cookie);
    Strcat_charp(tmp, "\r\n");
    /* [DRAFT 12] s. 10.1 */
    if (cookie->ptr[0] != '$')
      Strcat_charp(tmp, "Cookie2: $Version=\"1\"\r\n");
  }
  if (this->method == HttpMethod::POST) {
    if (this->request->enctype == FORM_ENCTYPE_MULTIPART) {
      Strcat_charp(tmp, "Content-Type: multipart/form-data; boundary=");
      Strcat_charp(tmp, this->request->boundary);
      Strcat_charp(tmp, "\r\n");
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", this->request->length));
      Strcat_charp(tmp, "\r\n");
    } else {
      if (!override_content_type) {
        Strcat_charp(tmp,
                     "Content-Type: application/x-www-form-urlencoded\r\n");
      }
      Strcat(tmp, Sprintf("Content-Length: %ld\r\n", this->request->length));
      if (header_string)
        Strcat(tmp, header_string);
      Strcat_charp(tmp, "\r\n");
      Strcat_charp_n(tmp, this->request->body, this->request->length);
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

void HttpRequest::add_auth_cookie() {
  if (this->add_auth_cookie_flag && this->realm && this->uname && this->pwd) {
    /* If authorization is required and passed */
    add_auth_user_passwd(url, qstr_unquote(this->realm)->ptr, this->uname,
                         this->pwd, 0);
    this->add_auth_cookie_flag = false;
  }
}
