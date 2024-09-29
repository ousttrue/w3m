#include "http_response.h"
#include "datetime.h"
#include "func.h"
#include "cookie.h"
#include "etc.h"
#include "buffer.h"
#include "termsize.h"
#include "istream.h"
#include "mimehead.h"
#include "indep.h"
#include "alloc.h"
#include "app.h"
#include "url.h"
#include "url_stream.h"
#include "buffer.h"
#include "myctype.h"
#include "terms.h"
#include <stdint.h>

static int http_response_code;
int64_t current_content_length;
int ShowEffect = true;

Str checkType(Str s, Lineprop **oprop) {
  Lineprop mode;
  Lineprop effect = PE_NORMAL;
  Lineprop *prop;
  static Lineprop *prop_buffer = NULL;
  static int prop_size = 0;
  char *str = s->ptr, *endp = &s->ptr[s->length], *bs = NULL;
  int do_copy = false;
  int i;
  int plen = 0, clen;

  if (prop_size < s->length) {
    prop_size = (s->length > LINELEN) ? s->length : LINELEN;
    prop_buffer = New_Reuse(Lineprop, prop_buffer, prop_size);
  }
  prop = prop_buffer;

  if (ShowEffect) {
    bs = memchr(str, '\b', s->length);
    if ((bs != NULL)) {
      char *sp = str, *ep;
      s = Strnew_size(s->length);
      do_copy = true;
      ep = bs ? (bs - 2) : endp;
      for (; str < ep && IS_ASCII(*str); str++) {
        *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
      }
      Strcat_charp_n(s, sp, (int)(str - sp));
    }
  }
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++)
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
  }

  while (str < endp) {
    if (prop - prop_buffer >= prop_size)
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = memchr(str, '\b', endp - str);
        continue;
      } else if (str == bs) {
        if (*(str + 1) == '_') {
          if (s->length) {
            str += 2;
            *(prop - 1) |= PE_UNDER;
          } else {
            str++;
          }
        } else {
          if (s->length) {
            if (*(str - 1) == *(str + 1)) {
              *(prop - 1) |= PE_BOLD;
              str += 2;
            } else {
              Strshrink(s, 1);
              prop--;
              str++;
            }
          } else {
            str++;
          }
        }
        if (str < endp)
          bs = memchr(str, '\b', endp - str);
        continue;
      }
    }

    plen = get_mclen(str);
    mode = get_mctype(str) | effect;
    *(prop++) = mode;
    {
      if (do_copy)
        Strcat_char(s, (char)*str);
      str++;
    }
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}

/* This array should be somewhere else */
/* FIXME: gettextize? */
const char *violations[COO_EMAX] = {
    "internal error",          "tail match failed",
    "wrong number of dots",    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1", "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",   "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

int readHeader(struct URLFile *uf, struct Buffer *newBuf, int thru,
               struct Url *pu) {

  struct TextList *headerlist = newBuf->document_header = newTextList();
  if (uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  FILE *src = nullptr;
  if (thru && !newBuf->header_source) {
    char *tmpf = tmpfname(TMPF_DFL, nullptr)->ptr;
    src = fopen(tmpf, "w");
    if (src)
      newBuf->header_source = tmpf;
  }

  char *p, *q;
  char *emsg;
  char c;
  Str lineBuf2 = nullptr;
  Str tmp;
  Lineprop *propBuffer;
  while ((tmp = StrmyUFgets(uf))->length) {
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
      c = UFgetc(uf);
      UFundogetc(uf);
      if (c == ' ' || c == '\t')
        /* header line is continued */
        continue;
      lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
      lineBuf2 = convertLine(lineBuf2, RAW_MODE);
      /* separated with line and stored */
      tmp = Strnew_size(lineBuf2->length);
      for (p = lineBuf2->ptr; *p; p = q) {
        for (q = p; *q && *q != '\r' && *q != '\n'; q++)
          ;
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
        Strcat(tmp, lineBuf2);
        if (thru)
          addnewline(newBuf, lineBuf2->ptr, propBuffer, lineBuf2->length,
                     FOLD_BUFFER_WIDTH, -1);
        for (; *q && (*q == '\r' || *q == '\n'); q++)
          ;
      }
      lineBuf2 = tmp;
    }
    if ((uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS) &&
        http_response_code == -1) {
      p = lineBuf2->ptr;
      while (*p && !IS_SPACE(*p))
        p++;
      while (*p && IS_SPACE(*p))
        p++;
      http_response_code = atoi(p);
      term_message(lineBuf2->ptr);
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
      struct compression_decoder *d;
      p = lineBuf2->ptr + 17;
      while (IS_SPACE(*p))
        p++;

      uf->compression = compressionFromEncoding(p);
      uf->content_encoding = uf->compression;
    } else if (use_cookie && accept_cookie && pu &&
               check_cookie_accept_domain(pu->host) &&
               (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {
      Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
          comment = NULL, commentURL = NULL, port = NULL, tmp2;
      int version, quoted, flag = 0;
      time_t expires = (time_t)-1;

      q = NULL;
      if (lineBuf2->ptr[10] == '2') {
        p = lineBuf2->ptr + 12;
        version = 1;
      } else {
        p = lineBuf2->ptr + 11;
        version = 0;
      }
#ifdef DEBUG
      fprintf(stderr, "Set-Cookie: [%s]\n", p);
#endif /* DEBUG */
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
        if (matchattr(p, "expires", 7, &tmp2)) {
          /* version 0 */
          expires = mymktime(tmp2->ptr);
        } else if (matchattr(p, "max-age", 7, &tmp2)) {
          /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2
           */
          expires = time(NULL) + atol(tmp2->ptr);
        } else if (matchattr(p, "domain", 6, &tmp2)) {
          domain = tmp2;
        } else if (matchattr(p, "path", 4, &tmp2)) {
          path = tmp2;
        } else if (matchattr(p, "secure", 6, NULL)) {
          flag |= COO_SECURE;
        } else if (matchattr(p, "comment", 7, &tmp2)) {
          comment = tmp2;
        } else if (matchattr(p, "version", 7, &tmp2)) {
          version = atoi(tmp2->ptr);
        } else if (matchattr(p, "port", 4, &tmp2)) {
          /* version 1, Set-Cookie2 */
          port = tmp2;
        } else if (matchattr(p, "commentURL", 10, &tmp2)) {
          /* version 1, Set-Cookie2 */
          commentURL = tmp2;
        } else if (matchattr(p, "discard", 7, NULL)) {
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
            disp_message_nsec("Received a secured cookie", false, 1, true,
                              false);
          else
            disp_message_nsec(
                Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr,
                false, 1, true, false);
        }
        err = add_cookie(pu, name, value, expires, domain, path, flag, comment,
                         version, port, commentURL);
        if (err) {
          char *ans =
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
            disp_message_nsec(Sprintf("Accepting invalid cookie: %s=%s",
                                      name->ptr, value->ptr)
                                  ->ptr,
                              false, 1, true, false);
        }
      }
    } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
               uf->scheme == SCM_LOCAL_CGI) {
      Str funcname = Strnew();
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
    if (headerlist)
      pushText(headerlist, lineBuf2->ptr);
    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
  if (thru)
    addnewline(newBuf, "", propBuffer, 0, -1, -1);
  if (src)
    fclose(src);

  return http_response_code;
}

char *checkHeader(struct Buffer *buf, char *field) {
  int len;
  struct TextListItem *i;
  char *p;

  if (buf == NULL || field == NULL || buf->document_header == NULL)
    return NULL;
  len = strlen(field);
  for (i = buf->document_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, field, len)) {
      p = i->ptr + len;
      return remove_space(p);
    }
  }
  return NULL;
}

char *checkContentType(struct Buffer *buf) {
  char *p;
  Str r;
  p = checkHeader(buf, "Content-Type:");
  if (p == NULL)
    return NULL;
  r = Strnew();
  while (*p && *p != ';' && !IS_SPACE(*p))
    Strcat_char(r, *p++);
  return r->ptr;
}
