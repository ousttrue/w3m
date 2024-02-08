#include "http_response.h"
#include "matchattr.h"
#include "app.h"
#include "screen.h"
#include "cookie.h"
#include "Str.h"
#include "message.h"
#include "textlist.h"
#include "w3m.h"
#include "myctype.h"
#include "url_stream.h"
#include "readbuffer.h"
#include "mimehead.h"
#include "terms.h"
#include "func.h"
#include <strings.h>
#include <unistd.h>

int FollowRedirection = 10;

HttpResponse::~HttpResponse() {
  if (this->sourcefile.size() &&
      (!this->real_type || strncasecmp(this->real_type, "image/", 6))) {
  }
}

static bool same_url_p(const Url &pu1, const Url &pu2) {
  return (pu1.schema == pu2.schema && pu1.port == pu2.port &&
          (pu1.host.size() ? pu2.host.size() ? pu1.host == pu2.host : false
                           : true) &&
          (pu1.file.size() ? pu2.file.size() ? pu1.file == pu2.file : false
                           : true));
}

bool HttpResponse::checkRedirection(const Url &pu) {

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, pu.to_Str().c_str());
    disp_err_message(tmp->ptr, false);
    return false;
  }

  for (auto &url : redirectins) {
    if (same_url_p(pu, url)) {
      // same url found !
      auto tmp = Sprintf("Redirection loop detected (%s)", pu.to_Str().c_str());
      disp_err_message(tmp->ptr, false);
      return false;
    }
  }

  redirectins.push_back(pu);

  return true;
}

int HttpResponse::readHeader(UrlStream *uf, const Url &pu) {
  char *p, *q;
  char c;
  Str *lineBuf2 = NULL;

  // TextList *headerlist;
  FILE *src = NULL;

  this->document_header = newTextList();
  if (uf->schema == SCM_HTTP || uf->schema == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  while (true) {
    auto _tmp = uf->stream->StrmyISgets();
    if (_tmp.empty()) {
      break;
    }
    auto tmp = Strnew(_tmp);
    if (w3m_reqlog) {
      FILE *ff;
      ff = fopen(w3m_reqlog, "a");
      if (ff) {
        Strfputs(tmp, ff);
        fclose(ff);
      }
    }
    if (src)
      Strfputs(tmp, src);
    cleanup_line(tmp, HEADER_MODE);
    if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0') {
      if (!lineBuf2)
        /* there is no header */
        break;
      /* last header */
    } else {
      if (lineBuf2) {
        Strcat(lineBuf2, tmp);
      } else {
        lineBuf2 = tmp;
      }

      c = uf->stream ? uf->stream->ISgetc() : '\0';

      uf->stream->ISundogetc();
      if (c == ' ' || c == '\t')
        /* header line is continued */
        continue;
      lineBuf2 = decodeMIME0(lineBuf2);
      cleanup_line(lineBuf2, RAW_MODE);
      /* separated with line and stored */
      tmp = Strnew_size(lineBuf2->length);
      for (p = lineBuf2->ptr; *p; p = q) {
        for (q = p; *q && *q != '\r' && *q != '\n'; q++)
          ;
        Lineprop *propBuffer;
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
        Strcat(tmp, lineBuf2);
        for (; *q && (*q == '\r' || *q == '\n'); q++)
          ;
      }
      lineBuf2 = tmp;
    }

    if ((uf->schema == SCM_HTTP || uf->schema == SCM_HTTPS) &&
        http_response_code == -1) {
      p = lineBuf2->ptr;
      while (*p && !IS_SPACE(*p))
        p++;
      while (*p && IS_SPACE(*p))
        p++;
      http_response_code = atoi(p);
      if (fmInitialized) {
        message(lineBuf2->ptr, 0, 0);
        refresh(term_io());
      }
    }
    if (!strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26)) {
      p = lineBuf2->ptr + 26;
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
    } else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {

      process_compression(lineBuf2, uf);

    } else if (use_cookie && accept_cookie &&
               check_cookie_accept_domain(pu.host.c_str()) &&
               (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {

      process_http_cookie(&pu, lineBuf2);

    } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
               uf->schema == SCM_LOCAL_CGI) {
      Str *funcname = Strnew();
      int f;

      p = lineBuf2->ptr + 12;
      SKIP_BLANKS(p);
      while (*p && !IS_SPACE(*p))
        Strcat_char(funcname, *(p++));
      SKIP_BLANKS(p);
      f = getFuncList(funcname->ptr);
      if (f >= 0) {
        tmp = Strnew_charp(p);
        Strchop(tmp);
        pushEvent(f, tmp->ptr);
      }
    }
    pushText(document_header, lineBuf2->ptr);
    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
  if (src) {
    fclose(src);
  }

  return http_response_code;
}

const char *HttpResponse::getHeader(const char *field) const {
  if (field == NULL || this->document_header == NULL) {
    return NULL;
  }

  auto len = strlen(field);
  for (auto i = this->document_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, field, len)) {
      auto p = i->ptr + len;
      return remove_space(p);
    }
  }
  return NULL;
}

const char *mybasename(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  while (s <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s;
  return allocStr(p, -1);
}

#define DEF_SAVE_FILE "index.html"
const char *guess_filename(const char *file) {
  const char *p = NULL, *s;

  if (file != NULL)
    p = mybasename(file);
  if (p == NULL || *p == '\0')
    return DEF_SAVE_FILE;
  s = p;
  if (*p == '#')
    p++;
  while (*p != '\0') {
    if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
      *(char *)p = '\0';
      break;
    }
    p++;
  }
  return s;
}

const char *HttpResponse::guess_save_name(const char *path) const {
  if (this->document_header) {
    Str *name = NULL;
    const char *p, *q;
    if ((p = this->getHeader("Content-Disposition:")) != NULL &&
        (q = strcasestr(p, "filename")) != NULL &&
        (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
        matchattr(q, "filename", 8, &name))
      path = name->ptr;
    else if ((p = this->getHeader("Content-Type:")) != NULL &&
             (q = strcasestr(p, "name")) != NULL &&
             (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
             matchattr(q, "name", 4, &name))
      path = name->ptr;
  }
  return guess_filename(path);
}
