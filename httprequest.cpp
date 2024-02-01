#include "httprequest.h"
#include "w3m.h"
#include "Str.h"
#include "url.h"
#include "textlist.h"
#include "rc.h"
#include "cookie.h"
#include "form.h"
#include "myctype.h"

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

Str *HTTPrequestMethod(HRequest *hr) {
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

Str *HTTPrequestURI(ParsedURL *pu, HRequest *hr) {
  Str *tmp = Strnew();
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

static Str *parsedURL2RefererOriginStr(ParsedURL *pu) {
  auto f = pu->file;
  auto q = pu->query;
  pu->file = NULL;
  pu->query = NULL;
  auto s = _parsedURL2Str(pu, false, false, false);
  pu->file = f;
  pu->query = q;
  return s;
}

static char *otherinfo(ParsedURL *target, ParsedURL *current, char *referer) {
  Str *s = Strnew();
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
    if (target->port != getDefaultPort(target->schema)) {
      Strcat(s, Sprintf(":%d", target->port));
    }
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
         current->port != target->port || current->schema != target->schema))
      cross_origin = true;
    if (current && current->schema == SCM_HTTPS &&
        target->schema != SCM_HTTPS) {
      /* Don't send Referer: if https:// -> http:// */
    } else if (referer == NULL && current && current->schema != SCM_LOCAL &&
               current->schema != SCM_LOCAL_CGI &&
               current->schema != SCM_DATA &&
               (current->schema != SCM_FTP ||
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

Str *HTTPrequest(ParsedURL *pu, ParsedURL *current, HRequest *hr,
                 TextList *extra) {
  Str *tmp;
  TextListItem *i;
  Str *cookie;
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
        if (pu->schema == SCM_HTTPS && hr->command != HR_COMMAND_CONNECT)
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

bool matchattr(const char *p, const char *attr, int len, Str **value) {
  int quoted;
  const char *q = NULL;
  if (strncasecmp(p, attr, len) == 0) {
    p += len;
    SKIP_BLANKS(p);
    if (value) {
      *value = Strnew();
      if (*p == '=') {
        p++;
        SKIP_BLANKS(p);
        quoted = 0;
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
