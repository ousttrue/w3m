#include "http_request.h"
#include "cmp.h"
#include "form.h"
#include "quote.h"
#include "url.h"
#include "cookie.h"
#include "auth_pass.h"
#include <sstream>
#ifdef _MSC_VER
#include <io.h>
#endif

bool override_user_agent = false;
std::string UserAgent;
std::string AcceptLang;
std::string AcceptEncoding;
std::string AcceptMedia;
bool NoCache = false;
bool NoSendReferer = false;
bool CrossOriginReferer = true;
bool override_content_type = false;
std::string header_string;

std::string HttpRequest::getRequestURI() const {
  std::stringstream tmp;
  if (this->method == HttpMethod::CONNECT) {
    tmp << url.host;
    tmp << ":" << url.port;
  } else if (this->flag & HR_FLAG_LOCAL) {
    tmp << url.file;
    if (url.query.size()) {
      tmp << '?';
      tmp << url.query;
    }
  } else {
    tmp << url.to_Str(true, true, false);
  }
  return tmp.str();
}

static std::string otherinfo(const Url &target, std::optional<Url> current,
                             const HttpOption &option,
                             const char *w3m_version) {
  std::stringstream s;
  // const char *url_user_agent = nullptr;
  if (!override_user_agent) {
    s << "User-Agent: ";
    // if (url_user_agent)
    //   s << url_user_agent;
    if (UserAgent.empty())
      s << w3m_version;
    else
      s << UserAgent;
    s << "\r\n";
  }

  s << "Accept: " << AcceptMedia << "\r\n";
  s << "Accept-Encoding: " << AcceptEncoding << "\r\n";
  s << "Accept-Language: " << AcceptLang << "\r\n";

  if (target.host.size()) {
    s << "Host: " << target.host;
    if (target.port != getDefaultPort(target.scheme)) {
      s << ":" << target.port;
    }
    s << "\r\n";
  }

  if (option.no_cache || NoCache) {
    s << "Pragma: no-cache\r\n";
    s << "Cache-control: no-cache\r\n";
  }

  auto no_referer = NoSendReferer;
  const int *no_referer_ptr = nullptr;
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  no_referer_ptr = nullptr;
  no_referer = no_referer || (no_referer_ptr && *no_referer_ptr);
  if (!no_referer) {
    int cross_origin = false;
    if (CrossOriginReferer && current && current->host.size() &&
        (target.host.empty() || current->host != target.host ||
         current->port != target.port || current->scheme != target.scheme)) {
      cross_origin = true;
    }
    if (current && current->scheme == SCM_HTTPS && target.scheme != SCM_HTTPS) {
      /* Don't send Referer: if https:// -> http:// */
    } else if (option.referer.empty() && current &&
               current->scheme != SCM_LOCAL &&
               current->scheme != SCM_LOCAL_CGI &&
               current->scheme != SCM_DATA) {
      s << "Referer: ";
      if (cross_origin)
        s << current->RefererOriginStr();
      else
        s << current->to_RefererStr();
      s << "\r\n";
    } else if (option.referer.size() && !option.no_referer) {
      s << "Referer: ";
      if (cross_origin)
        s << current->RefererOriginStr();
      else
        s << option.referer;
      s << "\r\n";
    }
  }
  return s.str();
}

std::string HttpRequest::to_Str(const char *w3m_version) const {

  std::stringstream tmp;
  tmp << to_str(this->method);
  tmp << " ";
  tmp << this->getRequestURI();
  tmp << " HTTP/1.0\r\n";
  if (this->option.no_referer) {
    tmp << otherinfo(url, {}, this->option, w3m_version);
  } else {
    tmp << otherinfo(url, current, this->option, w3m_version);
  }
  for (auto &i : extra_headers) {
    if (strncasecmp(i.c_str(),
                    "Authorization:", sizeof("Authorization:") - 1) == 0) {
      if (this->method == HttpMethod::CONNECT)
        continue;
    }
    if (strncasecmp(i.c_str(), "Proxy-Authorization:",
                    sizeof("Proxy-Authorization:") - 1) == 0) {
      if (url.scheme == SCM_HTTPS && this->method != HttpMethod::CONNECT)
        continue;
    }
    tmp << i;
  }

  std::optional<std::string> cookie;
  if (this->method != HttpMethod::CONNECT && use_cookie &&
      (cookie = find_cookie(url)) && cookie->size()) {
    tmp << "Cookie: ";
    tmp << *cookie;
    tmp << "\r\n";
    /* [DRAFT 12] s. 10.1 */
    if ((*cookie)[0] != '$') {
      tmp << "Cookie2: $Version=\"1\"\r\n";
    }
  }

  if (this->method == HttpMethod::POST) {
    if (this->request->enctype == FORM_ENCTYPE_MULTIPART) {
      tmp << "Content-Type: multipart/form-data; boundary=";
      tmp << this->request->boundary;
      tmp << "\r\n";
      tmp << "Content-Length: " << this->request->length << "\r\n";
      tmp << "\r\n";
    } else {
      if (!override_content_type) {
        tmp << "Content-Type: application/x-www-form-urlencoded\r\n";
      }
      tmp << "Content-Length: " << this->request->length << "\r\n";
      if (header_string.size()) {
        tmp << header_string;
      }
      tmp << "\r\n";
      tmp << this->request->body.substr(0, this->request->length);
      tmp << "\r\n";
    }
  } else {
    if (header_string.size())
      tmp << header_string;
    tmp << "\r\n";
  }

  return tmp.str();
}

void HttpRequest::add_auth_cookie() {
  if (this->add_auth_cookie_flag && this->realm.size() && this->uname.size() &&
      this->pwd.size()) {
    /* If authorization is required and passed */
    add_auth_user_passwd(url, qstr_unquote(this->realm), this->uname, this->pwd,
                         0);
    this->add_auth_cookie_flag = false;
  }
}
