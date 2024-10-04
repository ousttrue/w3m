#include "http_response.h"
#include "text.h"
#include "datetime.h"
#include "http_cookie.h"
#include "core.h"
#include "func.h"
#include "http_cookie.h"
#include "buffer.h"
#include "termsize.h"
#include "istream.h"
#include "mimehead.h"
#include "alloc.h"
#include "url.h"
#include "url_stream.h"
#include "buffer.h"
#include "myctype.h"
#include "terms.h"
#include <stdint.h>

/* This array should be somewhere else */
/* FIXME: gettextize? */
const char *violations[COO_EMAX] = {
    "internal error",          "tail match failed",
    "wrong number of dots",    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1", "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",   "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

void httpReadHeader(struct HttpResponse *res, struct URLFile *uf,
                    const char *key, const char *p, struct Url *pu) {
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
  } else if (!strcasecmp(key, "content-transfer-encoding")) {
    while (IS_SPACE(*p))
      p++;
    if (!strncasecmp(p, "base64", 6))
      uf->encoding = ENC_BASE64;
    else if (!strncasecmp(p, "quoted-printable", 16))
      uf->encoding = ENC_QUOTE;
    else if (!strncasecmp(p, "uuencode", 8) ||
             !strncasecmp(p, "x-uuencode", 10))
      uf->encoding = ENC_UUENCODE;
    else
      uf->encoding = ENC_7BIT;
  } else if (!strcasecmp(key, "content-encoding")) {
    while (IS_SPACE(*p)) {
      p++;
    }
    uf->compression = compressionFromEncoding(p);
    uf->content_encoding = uf->compression;
  } else if (use_cookie && accept_cookie && pu &&
             check_cookie_accept_domain(pu->host) &&
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
    if (pu && name->length > 0) {
      int err;
      if (show_cookie) {
        if (flag & COO_SECURE)
          disp_message_nsec("Received a secured cookie", false, 1, true, false);
        else
          disp_message_nsec(
              Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr,
              false, 1, true, false);
      }
      err = add_cookie(pu, name, value, expires, domain, path, flag, comment,
                       version, port, commentURL);
      if (err) {
        const char *ans =
            (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT) ? "y" : NULL;
        if ((err & COO_OVERRIDE_OK) &&
            accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK) {
          Str msg = Sprintf(
              "Accept bad cookie from %s for %s?", pu->host,
              ((domain && domain->ptr) ? domain->ptr : "<localdomain>"));
          if (msg->length > COLS - 10)
            Strshrink(msg, msg->length - (COLS - 10));
          Strcat_charp(msg, " (y/n)");
          ans = term_inputAnswer(msg->ptr);
        }
        if (ans == NULL || TOLOWER(*ans) != 'y' ||
            (err = add_cookie(pu, name, value, expires, domain, path,
                              flag | COO_OVERRIDE, comment, version, port,
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
          term_err_message(emsg);
          if (show_cookie)
            disp_message_nsec(emsg, false, 1, true, false);
        } else if (show_cookie)
          disp_message_nsec(
              Sprintf("Accepting invalid cookie: %s=%s", name->ptr, value->ptr)
                  ->ptr,
              false, 1, true, false);
      }
    }
  } else if (!strcasecmp(key, "w3m-control") && uf->scheme == SCM_LOCAL_CGI) {
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

struct HttpResponse *httpReadResponse(struct URLFile *uf, struct Url *pu) {
  auto res = New(struct HttpResponse);
  res->content_type = CONTENTTYPE_TextPlane;
  res->content_charset = CHARSET_ISO_8859_1;
  res->content_length = 0;
  res->document_header = newTextList();
  res->http_status_code = -1;

  Str line;
  if (uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS) {
    // HTTP/1.1 404 Not Found
    line = StrmyUFgets(uf);
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
  while ((line = StrmyUFgets(uf))->length) {
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
    char c = UFgetc(uf);
    UFundogetc(uf);
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

      httpReadHeader(res, uf, lineBuf2->ptr, value, pu);
    }

    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }

  return res;
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
