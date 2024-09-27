#include "file.h"
#include "app.h"
#include "cookie.h"
#include "etc.h"
#include "html.h"
#include "ctrlcode.h"
#include "os.h"
#include "http_request.h"
#include "url_stream.h"
#include "buffer.h"
#include "map.h"
#include "readbuffer.h"
#include "tty.h"
#include "digest_auth.h"
#include "termsize.h"
#include "terms.h"
#include <sys/types.h>
#include "myctype.h"
#include <signal.h>
#include <setjmp.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
/* foo */

#include "html.h"
#include "parsetagx.h"
#include "localcgi.h"
#include "regex.h"

static JMP_BUF AbortLoading;
static MySignalHandler KeyAbort(SIGNAL_ARG) { LONGJMP(AbortLoading, 1); }

static char *guess_filename(char *file);
static FILE *lessopen_stream(char *path);

static int http_response_code;

int64_t current_content_length;

/* This array should be somewhere else */
/* FIXME: gettextize? */
char *violations[COO_EMAX] = {
    "internal error",          "tail match failed",
    "wrong number of dots",    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1", "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",   "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};

/* *INDENT-OFF* */
static struct compression_decoder {
  int type;
  char *ext;
  char *mime_type;
  int auxbin_p;
  char *cmd;
  char *name;
  char *encoding;
  char *encodings[4];
  int use_d_arg;
} compression_decoders[] = {
    {CMP_COMPRESS,
     ".gz",
     "application/x-gzip",
     0,
     GUNZIP_CMDNAME,
     GUNZIP_NAME,
     "gzip",
     {"gzip", "x-gzip", NULL},
     0},
    {CMP_COMPRESS,
     ".Z",
     "application/x-compress",
     0,
     GUNZIP_CMDNAME,
     GUNZIP_NAME,
     "compress",
     {"compress", "x-compress", NULL},
     0},
    {CMP_BZIP2,
     ".bz2",
     "application/x-bzip",
     0,
     BUNZIP2_CMDNAME,
     BUNZIP2_NAME,
     "bzip, bzip2",
     {"x-bzip", "bzip", "bzip2", NULL},
     0},
    {CMP_DEFLATE,
     ".deflate",
     "application/x-deflate",
     1,
     INFLATE_CMDNAME,
     INFLATE_NAME,
     "deflate",
     {"deflate", "x-deflate", NULL},
     0},
    {CMP_BROTLI,
     ".br",
     "application/x-br",
     0,
     BROTLI_CMDNAME,
     BROTLI_NAME,
     "br",
     {"br", "x-br", NULL},
     1},
    {CMP_NOCOMPRESS, NULL, NULL, 0, NULL, NULL, NULL, {NULL}, 0},
};
/* *INDENT-ON* */

#define SAVE_BUF_SIZE 1536

static void UFhalfclose(struct URLFile *f) {
  switch (f->scheme) {
  case SCM_FTP:
    closeFTP();
    break;
  default:
    UFclose(f);
    break;
  }
}

int currentLn(struct Buffer *buf) {
  if (buf->currentLine)
    /*     return buf->currentLine->real_linenumber + 1;      */
    return buf->currentLine->linenumber + 1;
  else
    return 1;
}

int dir_exist(char *path) {
  struct stat stbuf;

  if (path == NULL || *path == '\0')
    return 0;
  if (stat(path, &stbuf) == -1)
    return 0;
  return IS_DIRECTORY(stbuf.st_mode);
}

static int is_dump_text_type(char *type) {
  struct mailcap *mcap;
  return (type && (mcap = searchExtViewer(type)) &&
          (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int is_text_type(char *type) {
  return (type == NULL || type[0] == '\0' ||
          strncasecmp(type, "text/", 5) == 0 ||
          (strncasecmp(type, "application/", 12) == 0 &&
           strstr(type, "xhtml") != NULL) ||
          strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int is_plain_text_type(char *type) {
  return ((type && strcasecmp(type, "text/plain") == 0) ||
          (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(char *type) {
  return (type && (strcasecmp(type, "text/html") == 0 ||
                   strcasecmp(type, "application/xhtml+xml") == 0));
}

static void check_compression(char *path, struct URLFile *uf) {
  int len;
  struct compression_decoder *d;

  if (path == NULL)
    return;

  len = strlen(path);
  uf->compression = CMP_NOCOMPRESS;
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    int elen;
    if (d->ext == NULL)
      continue;
    elen = strlen(d->ext);
    if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0) {
      uf->compression = d->type;
      uf->guess_type = d->mime_type;
      break;
    }
  }
}

static char *compress_application_type(int compression) {
  struct compression_decoder *d;

  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->type == compression)
      return d->mime_type;
  }
  return NULL;
}

static char *uncompressed_file_type(char *path, char **ext) {
  int len, slen;
  Str fn;
  char *t0;
  struct compression_decoder *d;

  if (path == NULL)
    return NULL;

  slen = 0;
  len = strlen(path);
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (d->ext == NULL)
      continue;
    slen = strlen(d->ext);
    if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
      break;
  }
  if (d->type == CMP_NOCOMPRESS)
    return NULL;

  fn = Strnew_charp(path);
  Strshrink(fn, slen);
  if (ext)
    *ext = filename_extension(fn->ptr, 0);
  t0 = guessContentType(fn->ptr);
  if (t0 == NULL)
    t0 = "text/plain";
  return t0;
}

void examineFile(char *path, struct URLFile *uf) {
  struct stat stbuf;

  uf->guess_type = NULL;
  if (path == NULL || *path == '\0' || stat(path, &stbuf) == -1 ||
      NOT_REGULAR(stbuf.st_mode)) {
    uf->stream = NULL;
    return;
  }
  uf->stream = openIS(path);
  if (!do_download) {
    if (use_lessopen && getenv("LESSOPEN") != NULL) {
      FILE *fp;
      uf->guess_type = guessContentType(path);
      if (uf->guess_type == NULL)
        uf->guess_type = "text/plain";
      if (is_html_type(uf->guess_type))
        return;
      if ((fp = lessopen_stream(path))) {
        UFclose(uf);
        uf->stream = newFileStream(fp, (void (*)())pclose);
        uf->guess_type = "text/plain";
        return;
      }
    }
    check_compression(path, uf);
    if (uf->compression != CMP_NOCOMPRESS) {
      char *ext = uf->ext;
      char *t0 = uncompressed_file_type(path, &ext);
      uf->guess_type = t0;
      uf->ext = ext;
      uncompress_stream(uf, NULL);
      return;
    }
  }
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

int check_command(char *cmd, int auxbin_p) {
  static char *path = NULL;
  Str dirs;
  char *p, *np;
  Str pathname;
  struct stat st;

  if (path == NULL)
    path = getenv("PATH");
  if (auxbin_p)
    dirs = Strnew_charp(w3m_auxbin_dir());
  else
    dirs = Strnew_charp(path);
  for (p = dirs->ptr; p != NULL; p = np) {
    np = strchr(p, PATH_SEPARATOR);
    if (np)
      *np++ = '\0';
    pathname = Strnew();
    Strcat_charp(pathname, p);
    Strcat_char(pathname, '/');
    Strcat_charp(pathname, cmd);
    if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode) &&
        (st.st_mode & S_IXANY) != 0)
      return 1;
  }
  return 0;
}

char *acceptableEncoding() {
  static Str encodings = nullptr;
  if (encodings != nullptr)
    return encodings->ptr;

  struct TextList *l = newTextList();
  for (auto d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (check_command(d->cmd, d->auxbin_p)) {
      pushText(l, d->encoding);
    }
  }
  encodings = Strnew();
  char *p;
  while ((p = popText(l)) != nullptr) {
    if (encodings->length)
      Strcat_charp(encodings, ", ");
    Strcat_charp(encodings, p);
  }
  return encodings->ptr;
}

/*
 * convert line
 */
Str convertLine0(struct URLFile *uf, Str line, int mode) {
  if (mode != RAW_MODE)
    cleanup_line(line, mode);
  return line;
}

int matchattr(char *p, char *attr, int len, Str *value) {
  int quoted;
  char *q = NULL;

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

void readHeader(struct URLFile *uf, struct Buffer *newBuf, int thru,
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
      lineBuf2 = convertLine(NULL, lineBuf2, RAW_MODE,
                             mime_charset ? &mime_charset : &charset,
                             mime_charset ? mime_charset : DocumentCharset);
      /* separated with line and stored */
      tmp = Strnew_size(lineBuf2->length);
      for (p = lineBuf2->ptr; *p; p = q) {
        for (q = p; *q && *q != '\r' && *q != '\n'; q++)
          ;
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer, NULL);
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
      uf->compression = CMP_NOCOMPRESS;
      for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
        char **e;
        for (e = d->encodings; *e != NULL; e++) {
          if (strncasecmp(p, *e, strlen(*e)) == 0) {
            uf->compression = d->type;
            break;
          }
        }
        if (uf->compression != CMP_NOCOMPRESS)
          break;
      }
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
            disp_message_nsec("Received a secured cookie", FALSE, 1, TRUE,
                              FALSE);
          else
            disp_message_nsec(
                Sprintf("Received cookie: %s=%s", name->ptr, value->ptr)->ptr,
                FALSE, 1, TRUE, FALSE);
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
              disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
          } else if (show_cookie)
            disp_message_nsec(Sprintf("Accepting invalid cookie: %s=%s",
                                      name->ptr, value->ptr)
                                  ->ptr,
                              FALSE, 1, TRUE, FALSE);
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
}

char *checkHeader(struct Buffer *buf, char *field) {
  int len;
  TextListItem *i;
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

enum {
  AUTHCHR_NUL,
  AUTHCHR_SEP,
  AUTHCHR_TOKEN,
};

static int skip_auth_token(char **pp) {
  char *p;
  int first = AUTHCHR_NUL, typ;

  for (p = *pp;; ++p) {
    switch (*p) {
    case '\0':
      goto endoftoken;
    default:
      if ((unsigned char)*p > 037) {
        typ = AUTHCHR_TOKEN;
        break;
      }
      /* thru */
    case '\177':
    case '[':
    case ']':
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '?':
    case '=':
    case ' ':
    case '\t':
    case ',':
      typ = AUTHCHR_SEP;
      break;
    }

    if (!first)
      first = typ;
    else if (first != typ)
      break;
  }
endoftoken:
  *pp = p;
  return first;
}

static Str extract_auth_val(char **q) {
  unsigned char *qq = *(unsigned char **)q;
  int quoted = 0;
  Str val = Strnew();

  SKIP_BLANKS(qq);
  if (*qq == '"') {
    quoted = TRUE;
    Strcat_char(val, *qq++);
  }
  while (*qq != '\0') {
    if (quoted && *qq == '"') {
      Strcat_char(val, *qq++);
      break;
    }
    if (!quoted) {
      switch (*qq) {
      case '[':
      case ']':
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ';':
      case ':':
      case '\\':
      case '"':
      case '/':
      case '?':
      case '=':
      case ' ':
      case '\t':
        qq++;
      case ',':
        goto end_token;
      default:
        if (*qq <= 037 || *qq == 0177) {
          qq++;
          goto end_token;
        }
      }
    } else if (quoted && *qq == '\\')
      Strcat_char(val, *qq++);
    Strcat_char(val, *qq++);
  }
end_token:
  *q = (char *)qq;
  return val;
}

static char *extract_auth_param(char *q, struct auth_param *auth) {
  struct auth_param *ap;
  char *p;

  for (ap = auth; ap->name != NULL; ap++) {
    ap->val = NULL;
  }

  while (*q != '\0') {
    SKIP_BLANKS(q);
    for (ap = auth; ap->name != NULL; ap++) {
      size_t len;

      len = strlen(ap->name);
      if (strncasecmp(q, ap->name, len) == 0 &&
          (IS_SPACE(q[len]) || q[len] == '=')) {
        p = q + len;
        SKIP_BLANKS(p);
        if (*p != '=')
          return q;
        q = p + 1;
        ap->val = extract_auth_val(&q);
        break;
      }
    }
    if (ap->name == NULL) {
      /* skip unknown param */
      int token_type;
      p = q;
      if ((token_type = skip_auth_token(&q)) == AUTHCHR_TOKEN &&
          (IS_SPACE(*q) || *q == '=')) {
        SKIP_BLANKS(q);
        if (*q != '=')
          return p;
        q++;
        extract_auth_val(&q);
      } else
        return p;
    }
    if (*q != '\0') {
      SKIP_BLANKS(q);
      if (*q == ',')
        q++;
      else
        break;
    }
  }
  return q;
}

static Str AuthBasicCred(struct http_auth *ha, Str uname, Str pw,
                         struct Url *pu, struct HttpRequest *hr,
                         struct FormList *request) {
  Str s = Strdup(uname);
  Strcat_char(s, ':');
  Strcat(s, pw);
  return Strnew_m_charp(
      "Basic ", base64_encode((const unsigned char *)s->ptr, s->length)->ptr,
      NULL);
}

/* *INDENT-OFF* */
struct auth_param none_auth_param[] = {{NULL, NULL}};

struct auth_param basic_auth_param[] = {{"realm", NULL}, {NULL, NULL}};

/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    {1, "Basic ", basic_auth_param, AuthBasicCred},
#ifdef USE_DIGEST_AUTH
    {10, "Digest ", digest_auth_param, AuthDigestCred},
#endif
    {
        0,
        NULL,
        NULL,
        NULL,
    }};
/* *INDENT-ON* */

static struct http_auth *findAuthentication(struct http_auth *hauth,
                                            struct Buffer *buf,
                                            char *auth_field) {
  struct http_auth *ha;
  int len = strlen(auth_field), slen;
  TextListItem *i;
  char *p0, *p;

  memset(hauth, 0, sizeof(struct http_auth));
  for (i = buf->document_header->first; i != NULL; i = i->next) {
    if (strncasecmp(i->ptr, auth_field, len) == 0) {
      for (p = i->ptr + len; p != NULL && *p != '\0';) {
        SKIP_BLANKS(p);
        p0 = p;
        for (ha = &www_auth[0]; ha->scheme != NULL; ha++) {
          slen = strlen(ha->scheme);
          if (strncasecmp(p, ha->scheme, slen) == 0) {
            p += slen;
            SKIP_BLANKS(p);
            if (hauth->pri < ha->pri) {
              *hauth = *ha;
              p = extract_auth_param(p, hauth->param);
              break;
            } else {
              /* weak auth */
              p = extract_auth_param(p, none_auth_param);
            }
          }
        }
        if (p0 == p) {
          /* all unknown auth failed */
          int token_type;
          if ((token_type = skip_auth_token(&p)) == AUTHCHR_TOKEN &&
              IS_SPACE(*p)) {
            SKIP_BLANKS(p);
            p = extract_auth_param(p, none_auth_param);
          } else
            break;
        }
      }
    }
  }
  return hauth->scheme ? hauth : NULL;
}

static void getAuthCookie(struct http_auth *hauth, char *auth_header,
                          struct TextList *extra_header, struct Url *pu,
                          struct HttpRequest *hr, struct FormList *request,
                          Str *uname, Str *pwd) {
  Str ss = NULL;
  Str tmp;
  TextListItem *i;
  int a_found;
  int auth_header_len = strlen(auth_header);
  char *realm = NULL;
  int proxy;

  if (hauth)
    realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

  if (!realm)
    return;

  a_found = FALSE;
  for (i = extra_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, auth_header, auth_header_len)) {
      a_found = TRUE;
      break;
    }
  }
  proxy = !strncasecmp("Proxy-Authorization:", auth_header, auth_header_len);
  if (a_found) {
    /* This means that *-Authenticate: header is received after
     * Authorization: header is sent to the server.
     */
    term_message("Wrong username or password");
    sleepSeconds(1);
    /* delete Authenticate: header from extra_header */
    delText(extra_header, i);
    invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
  }
  *uname = NULL;
  *pwd = NULL;

  if (!a_found &&
      find_auth_user_passwd(pu, realm, (Str *)uname, (Str *)pwd, proxy)) {
    /* found username & password in passwd file */;
  } else {
    if (QuietMessage)
      return;
    /* input username and password */
    sleepSeconds(2);

#ifdef _WIN32
    return;
#else
    if (!term_inputAuth(realm, proxy, uname, pwd)) {
      return;
    }
#endif
  }
  ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
  if (ss) {
    tmp = Strnew_charp(auth_header);
    Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
    pushText(extra_header, tmp->ptr);
  } else {
    *uname = NULL;
    *pwd = NULL;
  }
  return;
}

static int same_url_p(struct Url *pu1, struct Url *pu2) {
  return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
          (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1) &&
          (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

static int checkRedirection(struct Url *pu) {
  static struct Url *puv = NULL;
  static int nredir = 0;
  static int nredir_size = 0;
  Str tmp;

  if (pu == NULL) {
    nredir = 0;
    nredir_size = 0;
    puv = NULL;
    return TRUE;
  }
  if (nredir >= FollowRedirection) {
    /* FIXME: gettextize? */
    tmp = Sprintf("Number of redirections exceeded %d at %s", FollowRedirection,
                  parsedURL2Str(pu)->ptr);
    disp_err_message(tmp->ptr, FALSE);
    return FALSE;
  } else if (nredir_size > 0 &&
             (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) ||
              (!(nredir % 2) &&
               same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    /* FIXME: gettextize? */
    tmp = Sprintf("Redirection loop detected (%s)", parsedURL2Str(pu)->ptr);
    disp_err_message(tmp->ptr, FALSE);
    return FALSE;
  }
  if (!puv) {
    nredir_size = FollowRedirection / 2 + 1;
    puv = New_N(struct Url, nredir_size);
    memset(puv, 0, sizeof(struct Url) * nredir_size);
  }
  copyParsedURL(&puv[nredir % nredir_size], pu);
  nredir++;
  return TRUE;
}

/*
 * loadGeneralFile: load file to buffer
 */
#define DO_EXTERNAL                                                            \
  ((struct Buffer * (*)(struct URLFile *, struct Buffer *)) doExternal)
struct Buffer *loadGeneralFile(char *path, struct Url *current, char *referer,
                               int flag, struct FormList *request) {
  struct URLFile f, *of = NULL;
  struct Url pu;
  struct Buffer *b = NULL;
  struct Buffer *(*proc)(struct URLFile *, struct Buffer *) = loadBuffer;
  char *tpath;
  char *t = "text/plain", *p, *real_type = NULL;
  struct Buffer *t_buf = NULL;
  int searchHeader = SearchHeader;
  int searchHeader_through = TRUE;
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;
  struct TextList *extra_header = newTextList();
  Str uname = NULL;
  Str pwd = NULL;
  Str realm = NULL;
  int add_auth_cookie_flag;
  unsigned char status = HTST_NORMAL;
  struct URLOption url_option;
  Str tmp;
  Str page = NULL;
  struct HttpRequest hr;
  struct Url *auth_pu;

  tpath = path;
  prevtrap = NULL;
  add_auth_cookie_flag = 0;

  checkRedirection(NULL);

load_doc: {
  const char *sc_redirect;
  parseURL2(tpath, &pu, current);
  sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
  if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
    tpath = (char *)sc_redirect;
    request = NULL;
    add_auth_cookie_flag = 0;
    current = New(struct Url);
    *current = pu;
    status = HTST_NORMAL;
    goto load_doc;
  }
}
  TRAP_OFF;
  url_option.referer = referer;
  url_option.flag = flag;
  f = openURL(tpath, &pu, current, &url_option, request, extra_header, of, &hr,
              &status);
  of = NULL;
  if (f.stream == NULL) {
    switch (f.scheme) {
    case SCM_LOCAL: {
      struct stat st;
      if (stat(pu.real_file, &st) < 0)
        return NULL;
      if (S_ISDIR(st.st_mode)) {
        if (UseExternalDirBuffer) {
          Str cmd = Sprintf("%s?dir=%s#current", DirBufferCommand, pu.file);
          b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0, NULL);
          if (b != NULL && b != NO_BUFFER) {
            copyParsedURL(&b->currentURL, &pu);
            b->filename = b->currentURL.real_file;
          }
          return b;
        } else {
          page = loadLocalDir(pu.real_file);
          t = "local:directory";
        }
      }
    } break;
    case SCM_FTPDIR:
      page = loadFTPDir(&pu, &charset);
      t = "ftp:directory";
      break;
    case SCM_UNKNOWN:
      /* FIXME: gettextize? */
      disp_err_message(Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr,
                       FALSE);
      break;
    }
    if (page && page->length > 0)
      goto page_loaded;
    return NULL;
  }

  if (status == HTST_MISSING) {
    TRAP_OFF;
    UFclose(&f);
    return NULL;
  }

  /* openURL() succeeded */
  if (SETJMP(AbortLoading) != 0) {
    /* transfer interrupted */
    TRAP_OFF;
    if (b)
      discardBuffer(b);
    UFclose(&f);
    return NULL;
  }

  b = NULL;
  if (f.is_cgi) {
    /* local CGI */
    searchHeader = TRUE;
    searchHeader_through = FALSE;
  }
  if (header_string)
    header_string = NULL;
  TRAP_ON;
  if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS ||
      (((pu.scheme == SCM_FTP && non_null(FTP_proxy))) && !Do_not_use_proxy &&
       !check_no_proxy(pu.host))) {

    term_message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
    if (t_buf == NULL)
      t_buf = newBuffer(INIT_BUFFER_WIDTH);
#if 0 /* USE_SSL */
	if (IStype(f.stream) == IST_SSL) {
	    Str s = ssl_get_certificate(f.stream, pu.host);
	    if (s == NULL)
		return NULL;
	    else
		t_buf->ssl_certificate = s->ptr;
	}
#endif
    readHeader(&f, t_buf, FALSE, &pu);
    if (((http_response_code >= 301 && http_response_code <= 303) ||
         http_response_code == 307) &&
        (p = checkHeader(t_buf, "Location:")) != NULL &&
        checkRedirection(&pu)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      tpath = url_encode(p, NULL, 0);
      request = NULL;
      UFclose(&f);
      current = New(struct Url);
      copyParsedURL(current, &pu);
      t_buf = newBuffer(INIT_BUFFER_WIDTH);
      t_buf->bufferprop |= BP_REDIRECTED;
      status = HTST_NORMAL;
      goto load_doc;
    }
    t = checkContentType(t_buf);
    if (t == NULL && pu.file != NULL) {
      if (!((http_response_code >= 400 && http_response_code <= 407) ||
            (http_response_code >= 500 && http_response_code <= 505)))
        t = guessContentType(pu.file);
    }
    if (t == NULL)
      t = "text/plain";
    if (add_auth_cookie_flag && realm && uname && pwd) {
      /* If authorization is required and passed */
      add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd, 0);
      add_auth_cookie_flag = 0;
    }
    if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL &&
        http_response_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auth_pu = &pu;
        getAuthCookie(&hauth, "Authorization:", extra_header, auth_pu, &hr,
                      request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          TRAP_OFF;
          goto page_loaded;
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        goto load_doc;
      }
    }
    if ((p = checkHeader(t_buf, "Proxy-Authenticate:")) != NULL &&
        http_response_code == 407) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auth_pu = schemeToProxy(pu.scheme);
        getAuthCookie(&hauth, "Proxy-Authorization:", extra_header, auth_pu,
                      &hr, request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          TRAP_OFF;
          goto page_loaded;
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        goto load_doc;
      }
    }
    /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

    if (status == HTST_CONNECT) {
      of = &f;
      goto load_doc;
    }

    f.modtime = mymktime(checkHeader(t_buf, "Last-Modified:"));
  } else if (pu.scheme == SCM_FTP) {
    check_compression(path, &f);
    if (f.compression != CMP_NOCOMPRESS) {
      char *t1 = uncompressed_file_type(pu.file, NULL);
      real_type = f.guess_type;
      if (t1)
        t = t1;
      else
        t = real_type;
    } else {
      real_type = guessContentType(pu.file);
      if (real_type == NULL)
        real_type = "text/plain";
      t = real_type;
    }
  } else if (pu.scheme == SCM_DATA) {
    t = f.guess_type;
  } else if (searchHeader) {
    searchHeader = SearchHeader = FALSE;
    if (t_buf == NULL)
      t_buf = newBuffer(INIT_BUFFER_WIDTH);
    readHeader(&f, t_buf, searchHeader_through, &pu);
    if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL &&
        checkRedirection(&pu)) {
      /* document moved */
      tpath = url_encode(remove_space(p), NULL, 0);
      request = NULL;
      UFclose(&f);
      add_auth_cookie_flag = 0;
      current = New(struct Url);
      copyParsedURL(current, &pu);
      t_buf = newBuffer(INIT_BUFFER_WIDTH);
      t_buf->bufferprop |= BP_REDIRECTED;
      status = HTST_NORMAL;
      goto load_doc;
    }
#ifdef AUTH_DEBUG
    if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auth_pu = &pu;
        getAuthCookie(&hauth, "Authorization:", extra_header, auth_pu, &hr,
                      request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          TRAP_OFF;
          goto page_loaded;
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        goto load_doc;
      }
    }
#endif /* defined(AUTH_DEBUG) */
    t = checkContentType(t_buf);
    if (t == NULL)
      t = "text/plain";
  } else if (DefaultType) {
    t = DefaultType;
    DefaultType = NULL;
  } else {
    t = guessContentType(pu.file);
    if (t == NULL)
      t = "text/plain";
    real_type = t;
    if (f.guess_type)
      t = f.guess_type;
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  f.guess_type = t;

page_loaded:
  if (page) {
    FILE *src;
    tmp = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmp->ptr, "w");
    if (src) {
      Str s;
      s = wc_Str_conv_strict(page, InnerCharset, charset);
      Strfputs(s, src);
      fclose(src);
    }
    if (do_download) {
      char *file;
      if (!src)
        return NULL;
      file = guess_filename(pu.file);
      doFileMove(tmp->ptr, file);
      return NO_BUFFER;
    }
    b = loadHTMLString(page);
    if (b) {
      copyParsedURL(&b->currentURL, &pu);
      b->real_scheme = pu.scheme;
      b->real_type = t;
      if (src)
        b->sourcefile = tmp->ptr;
    }
    return b;
  }

  if (real_type == NULL)
    real_type = t;
  proc = loadBuffer;

  current_content_length = 0;
  if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
    current_content_length = strtoclen(p);
  if (do_download) {
    /* download only */
    char *file;
    TRAP_OFF;
    if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
      f.stream = newEncodedStream(f.stream, f.encoding);
    if (pu.scheme == SCM_LOCAL) {
      struct stat st;
      if (PreserveTimestamp && !stat(pu.real_file, &st))
        f.modtime = st.st_mtime;
      file = conv_from_system(guess_save_name(NULL, pu.real_file));
    } else
      file = guess_save_name(t_buf, pu.file);
    if (doFileSave(f, file) == 0)
      UFhalfclose(&f);
    else
      UFclose(&f);
    return NO_BUFFER;
  }

  if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    uncompress_stream(&f, &pu.real_file);
  } else if (f.compression != CMP_NOCOMPRESS) {
    if (is_text_type(t) || searchExtViewer(t)) {
      if (t_buf == NULL)
        t_buf = newBuffer(INIT_BUFFER_WIDTH);
      uncompress_stream(&f, &t_buf->sourcefile);
      uncompressed_file_type(pu.file, &f.ext);
    } else {
      t = compress_application_type(f.compression);
      f.compression = CMP_NOCOMPRESS;
    }
  }

  if (is_html_type(t))
    proc = loadHTMLBuffer;
  else if (is_plain_text_type(t))
    proc = loadBuffer;
  else if (is_dump_text_type(t)) {
    if (!do_download && searchExtViewer(t) != NULL) {
      proc = DO_EXTERNAL;
    } else {
      TRAP_OFF;
      if (pu.scheme == SCM_LOCAL) {
        UFclose(&f);
        _doFileCopy(pu.real_file,
                    conv_from_system(guess_save_name(NULL, pu.real_file)),
                    TRUE);
      } else {
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
          f.stream = newEncodedStream(f.stream, f.encoding);
        if (doFileSave(f, guess_save_name(t_buf, pu.file)) == 0)
          UFhalfclose(&f);
        else
          UFclose(&f);
      }
      return NO_BUFFER;
    }
  }

  if (t_buf == NULL)
    t_buf = newBuffer(INIT_BUFFER_WIDTH);
  copyParsedURL(&t_buf->currentURL, &pu);
  t_buf->filename = pu.real_file ? pu.real_file
                    : pu.file    ? conv_to_system(pu.file)
                                 : NULL;
  t_buf->ssl_certificate = f.ssl_certificate;
  if (proc == DO_EXTERNAL) {
    b = doExternal(f, t, t_buf);
  } else {
    b = loadSomething(&f, proc, t_buf);
  }
  UFclose(&f);
  if (b && b != NO_BUFFER) {
    b->real_scheme = f.scheme;
    b->real_type = real_type;
    if (pu.label) {
      if (proc == loadHTMLBuffer) {
        Anchor *a;
        a = searchURLLabel(b, pu.label);
        if (a != NULL) {
          gotoLine(b, a->start.line);
          if (label_topline)
            b->topLine = lineSkip(
                b, b->topLine,
                b->currentLine->linenumber - b->topLine->linenumber, FALSE);
          b->pos = a->start.pos;
          arrangeCursor(b);
        }
      } else { /* plain text */
        int l = atoi(pu.label);
        gotoRealLine(b, l);
        b->pos = 0;
        arrangeCursor(b);
      }
    }
  }
  if (header_string)
    header_string = NULL;
  if (b && b != NO_BUFFER)
    preFormUpdateBuffer(b);
  TRAP_OFF;
  return b;
}

#define TAG_IS(s, tag, len)                                                    \
  (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static int is_period_char(unsigned char *ch) {
  switch (*ch) {
  case ',':
  case '.':
  case ':':
  case ';':
  case '?':
  case '!':
  case ')':
  case ']':
  case '}':
  case '>':
    return 1;
  default:
    return 0;
  }
}

static int is_beginning_char(unsigned char *ch) {
  switch (*ch) {
  case '(':
  case '[':
  case '{':
  case '`':
  case '<':
    return 1;
  default:
    return 0;
  }
}

static int is_word_char(unsigned char *ch) {
  Lineprop ctype = get_mctype(ch);

  if (ctype == PC_CTRL)
    return 0;

  if (IS_ALNUM(*ch))
    return 1;

  switch (*ch) {
  case ',':
  case '.':
  case ':':
  case '\"': /* " */
  case '\'':
  case '$':
  case '%':
  case '*':
  case '+':
  case '-':
  case '@':
  case '~':
  case '_':
    return 1;
  }
  if (*ch == TIMES_CODE || *ch == DIVIDE_CODE || *ch == ANSP_CODE)
    return 0;
  if (*ch >= AGRAVE_CODE || *ch == NBSP_CODE)
    return 1;
  return 0;
}

int is_boundary(unsigned char *ch1, unsigned char *ch2) {
  if (!*ch1 || !*ch2)
    return 1;

  if (*ch1 == ' ' && *ch2 == ' ')
    return 0;

  if (*ch1 != ' ' && is_period_char(ch2))
    return 0;

  if (*ch2 != ' ' && is_beginning_char(ch1))
    return 0;

  if (is_word_char(ch1) && is_word_char(ch2))
    return 0;

  return 1;
}

void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                  int indent, int indent_incr, int width) {
  if (h_env->blank_lines == 0)
    flushline(h_env, obuf, indent, 1, width);
}

int getMetaRefreshParam(char *q, Str *refresh_uri) {
  int refresh_interval;
  char *r;
  Str s_tmp = NULL;

  if (q == NULL || refresh_uri == NULL)
    return 0;

  refresh_interval = atoi(q);
  if (refresh_interval < 0)
    return 0;

  while (*q) {
    if (!strncasecmp(q, "url=", 4)) {
      q += 4;
      if (*q == '\"' || *q == '\'') /* " or ' */
        q++;
      r = q;
      while (*r && !IS_SPACE(*r) && *r != ';')
        r++;
      s_tmp = Strnew_charp_n(q, r - q);

      if (s_tmp->length > 0 &&
          (s_tmp->ptr[s_tmp->length - 1] == '\"' ||  /* " */
           s_tmp->ptr[s_tmp->length - 1] == '\'')) { /* ' */
        s_tmp->length--;
        s_tmp->ptr[s_tmp->length] = '\0';
      }
      q = r;
    }
    while (*q && *q != ';')
      q++;
    if (*q == ';')
      q++;
    while (*q && *q == ' ')
      q++;
  }
  *refresh_uri = s_tmp;
  return refresh_interval;
}

extern char *NullLine;
extern Lineprop NullProp[];

static char *_size_unit[] = {"b",  "kb", "Mb", "Gb", "Tb", "Pb",
                             "Eb", "Zb", "Bb", "Yb", NULL};

char *convert_size(int64_t size, int usefloat) {
  float csize;
  int sizepos = 0;
  char **sizes = _size_unit;

  csize = (float)size;
  while (csize >= 999.495 && sizes[sizepos + 1]) {
    csize = csize / 1024.0;
    sizepos++;
  }
  return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
                 floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
      ->ptr;
}

char *convert_size2(int64_t size1, int64_t size2, int usefloat) {
  char **sizes = _size_unit;
  float csize, factor = 1;
  int sizepos = 0;

  csize = (float)((size1 > size2) ? size1 : size2);
  while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
    factor *= 1024.0;
    sizepos++;
  }
  return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
                 floor(size1 / factor * 100.0 + 0.5) / 100.0,
                 floor(size2 / factor * 100.0 + 0.5) / 100.0, sizes[sizepos])
      ->ptr;
}

static Str conv_symbol(Line *l) {
  Str tmp = NULL;
  char *p = l->lineBuf, *ep = p + l->len;
  Lineprop *pr = l->propBuf;
  char **symbol = get_symbol();

  for (; p < ep; p++, pr++) {
    if (*pr & PC_SYMBOL) {
      char c = *p - SYMBOL_BASE;
      if (tmp == NULL) {
        tmp = Strnew_size(l->len);
        Strcopy_charp_n(tmp, l->lineBuf, p - l->lineBuf);
      }
      Strcat_charp(tmp, symbol[(unsigned char)c % N_SYMBOL]);
    } else if (tmp != NULL)
      Strcat_char(tmp, *p);
  }
  if (tmp)
    return tmp;
  else
    return Strnew_charp_n(l->lineBuf, l->len);
}

/*
 * saveBuffer: write buffer to file
 */
static void _saveBuffer(struct Buffer *buf, Line *l, FILE *f, int cont) {
  Str tmp;
  int is_html = FALSE;

  is_html = is_html_type(buf->type);

  for (; l != NULL; l = l->next) {
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf, l->len);
    tmp = wc_Str_conv(tmp, InnerCharset, charset);
    Strfputs(tmp, f);
    if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
      putc('\n', f);
  }
}

void saveBuffer(struct Buffer *buf, FILE *f, int cont) {
  _saveBuffer(buf, buf->firstLine, f, cont);
}

void saveBufferBody(struct Buffer *buf, FILE *f, int cont) {
  Line *l = buf->firstLine;

  while (l != NULL && l->real_linenumber == 0)
    l = l->next;
  _saveBuffer(buf, l, f, cont);
}

struct Buffer *loadcmdout(char *cmd,
                          struct Buffer *(*loadproc)(struct URLFile *,
                                                     struct Buffer *),
                          struct Buffer *defaultbuf) {
  FILE *f, *popen(const char *, const char *);
  struct Buffer *buf;
  struct URLFile uf;

  if (cmd == NULL || *cmd == '\0')
    return NULL;
  f = popen(cmd, "r");
  if (f == NULL)
    return NULL;
  init_stream(&uf, SCM_UNKNOWN, newFileStream(f, (void (*)())pclose));
  buf = loadproc(&uf, defaultbuf);
  UFclose(&uf);
  return buf;
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
struct Buffer *getshell(char *cmd) {
  struct Buffer *buf;

  buf = loadcmdout(cmd, loadBuffer, NULL);
  if (buf == NULL)
    return NULL;
  buf->filename = cmd;
  buf->buffername =
      Sprintf("%s %s", SHELLBUFFERNAME, conv_from_system(cmd))->ptr;
  return buf;
}

int save2tmp(struct URLFile uf, char *tmpf) {
  FILE *ff;
  int check;
  int64_t linelen = 0, trbyte = 0;
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;
  static JMP_BUF env_bak;
  int retval = 0;
  char *buf = NULL;

  ff = fopen(tmpf, "wb");
  if (ff == NULL) {
    /* fclose(f); */
    return -1;
  }
  memcpy(env_bak, AbortLoading, sizeof(JMP_BUF));
  if (SETJMP(AbortLoading) != 0) {
    goto _end;
  }
  TRAP_ON;
  check = 0;
  {
    int count;

    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
      if (fwrite(buf, 1, count, ff) != count) {
        retval = -2;
        goto _end;
      }
      linelen += count;
      term_showProgress(&linelen, &trbyte, current_content_length);
    }
  }
_end:
  memcpy(AbortLoading, env_bak, sizeof(JMP_BUF));
  TRAP_OFF;
  xfree(buf);
  fclose(ff);
  current_content_length = 0;
  return retval;
}

int doFileMove(char *tmpf, char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

int checkCopyFile(char *path1, char *path2) {
  struct stat st1, st2;

  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int checkSaveFile(InputStream stream, char *path2) {
  struct stat st1, st2;
  int des = ISfileno(stream);

  if (des < 0)
    return 0;
  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int checkOverWrite(char *path) {
  struct stat st;
  char *ans;

  if (stat(path, &st) < 0)
    return 0;
  /* FIXME: gettextize? */
  ans = term_inputAnswer("File exists. Overwrite? (y/n)");
  if (ans && TOLOWER(*ans) == 'y')
    return 0;
  else
    return -1;
}

void uncompress_stream(struct URLFile *uf, char **src) {
  pid_t pid1;
  FILE *f1;
  char *expand_cmd = GUNZIP_CMDNAME;
  char *expand_name = GUNZIP_NAME;
  char *tmpf = NULL;
  char *ext = NULL;
  struct compression_decoder *d;
  int use_d_arg = 0;

  if (IStype(uf->stream) != IST_ENCODED) {
    uf->stream = newEncodedStream(uf->stream, uf->encoding);
    uf->encoding = ENC_7BIT;
  }
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (uf->compression == d->type) {
      if (d->auxbin_p)
        expand_cmd = auxbinFile(d->cmd);
      else
        expand_cmd = d->cmd;
      expand_name = d->name;
      ext = d->ext;
      use_d_arg = d->use_d_arg;
      break;
    }
  }
  uf->compression = CMP_NOCOMPRESS;

  if (uf->scheme != SCM_LOCAL) {
    tmpf = tmpfname(TMPF_DFL, ext)->ptr;
  }

  /* child1 -- stdout|f1=uf -> parent */
  pid1 = open_pipe_rw(&f1, NULL);
  if (pid1 < 0) {
    UFclose(uf);
    return;
  }
  if (pid1 == 0) {
    /* child */
    pid_t pid2;
    FILE *f2 = stdin;

    /* uf -> child2 -- stdout|stdin -> child1 */
    pid2 = open_pipe_rw(&f2, NULL);
    if (pid2 < 0) {
      UFclose(uf);
      exit(1);
    }
    if (pid2 == 0) {
      /* child2 */
      char *buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
      int count;
      FILE *f = NULL;

      setup_child(TRUE, 2, UFfileno(uf));
      if (tmpf)
        f = fopen(tmpf, "wb");
      while ((count = ISread_n(uf->stream, buf, SAVE_BUF_SIZE)) > 0) {
        if (fwrite(buf, 1, count, stdout) != count)
          break;
        if (f && fwrite(buf, 1, count, f) != count)
          break;
      }
      UFclose(uf);
      if (f)
        fclose(f);
      xfree(buf);
      exit(0);
    }
    /* child1 */
    dup2(1, 2); /* stderr>&stdout */
    setup_child(TRUE, -1, -1);
    if (use_d_arg)
      execlp(expand_cmd, expand_name, "-d", NULL);
    else
      execlp(expand_cmd, expand_name, NULL);
    exit(1);
  }
  if (tmpf) {
    if (src)
      *src = tmpf;
    else
      uf->scheme = SCM_LOCAL;
  }
  UFhalfclose(uf);
  uf->stream = newFileStream(f1, (void (*)())fclose);
}

static FILE *lessopen_stream(char *path) {
  char *lessopen;
  FILE *fp;

  lessopen = getenv("LESSOPEN");
  if (lessopen == NULL) {
    return NULL;
  }
  if (lessopen[0] == '\0') {
    return NULL;
  }

  if (lessopen[0] == '|') {
    /* pipe mode */
    Str tmpf;
    int c;

    ++lessopen;
    tmpf = Sprintf(lessopen, shell_quote(path));
    fp = popen(tmpf->ptr, "r");
    if (fp == NULL) {
      return NULL;
    }
    c = getc(fp);
    if (c == EOF) {
      pclose(fp);
      return NULL;
    }
    ungetc(c, fp);
  } else {
    /* filename mode */
    /* not supported m(__)m */
    fp = NULL;
  }
  return fp;
}

static char *guess_filename(char *file) {
  char *p = NULL, *s;

  if (file != NULL)
    p = mybasename(file);
  if (p == NULL || *p == '\0')
    return DEF_SAVE_FILE;
  s = p;
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

char *guess_save_name(struct Buffer *buf, char *path) {
  if (buf && buf->document_header) {
    Str name = NULL;
    char *p, *q;
    if ((p = checkHeader(buf, "Content-Disposition:")) != NULL &&
        (q = strcasestr(p, "filename")) != NULL &&
        (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
        matchattr(q, "filename", 8, &name))
      path = name->ptr;
    else if ((p = checkHeader(buf, "Content-Type:")) != NULL &&
             (q = strcasestr(p, "name")) != NULL &&
             (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
             matchattr(q, "name", 4, &name))
      path = name->ptr;
  }
  return guess_filename(path);
}

int _MoveFile(char *path1, char *path2) {
  InputStream f1;
  FILE *f2;
  int is_pipe;
  int64_t linelen = 0, trbyte = 0;
  char *buf = NULL;
  int count;

  f1 = openIS(path1);
  if (f1 == NULL)
    return -1;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = TRUE;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = FALSE;
    f2 = fopen(path2, "wb");
  }
  if (f2 == NULL) {
    ISclose(f1);
    return -1;
  }
  current_content_length = 0;
  buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
  while ((count = ISread_n(f1, buf, SAVE_BUF_SIZE)) > 0) {
    fwrite(buf, 1, count, f2);
    linelen += count;
    term_showProgress(&linelen, &trbyte, current_content_length);
  }
  xfree(buf);
  ISclose(f1);
  if (is_pipe)
    pclose(f2);
  else
    fclose(f2);
  return 0;
}
