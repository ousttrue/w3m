#include "file.h"
#include "message.h"
#include "screen.h"
#include "history.h"
#include "w3m.h"
#include "url.h"
#include "terms.h"
#include "tmpfile.h"
#include "alarm.h"
#include "linklist.h"
#include "httprequest.h"
#include "rc.h"
#include "linein.h"
#include "downloadlist.h"
#include "etc.h"
#include "proto.h"
#include "buffer.h"
#include "istream.h"
#include "textlist.h"
#include "funcname1.h"
#include "form.h"
#include "ctrlcode.h"
#include "readbuffer.h"
#include "cookie.h"
#include "anchor.h"
#include "display.h"
#include "fm.h"
#include "table.h"
#include "mailcap.h"
#include "signal_util.h"
#include <sys/types.h>
#include "myctype.h"
#include <setjmp.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
/* foo */

#include "html.h"
#include "parsetagx.h"
#include "local_cgi.h"
#include "regex.h"

int FollowRedirection = 10;

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
}

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

static char *guess_filename(char *file);
static int _MoveFile(char *path1, char *path2);
static void uncompress_stream(URLFile *uf, char **src);
static FILE *lessopen_stream(char *path);
static Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(URLFile *, Buffer *),
                          Buffer *defaultbuf);

static void addLink(Buffer *buf, struct parsed_tag *tag);

static int http_response_code;

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

static void UFhalfclose(URLFile *f) {
  switch (f->scheme) {
  case SCM_FTP:
    // closeFTP();
    break;
  default:
    UFclose(f);
    break;
  }
}

int currentLn(Buffer *buf) {
  if (buf->currentLine)
    /*     return buf->currentLine->real_linenumber + 1;      */
    return buf->currentLine->linenumber + 1;
  else
    return 1;
}

static Buffer *loadSomething(URLFile *f,
                             Buffer *(*loadproc)(URLFile *, Buffer *),
                             Buffer *defaultbuf) {
  Buffer *buf;

  if ((buf = loadproc(f, defaultbuf)) == NULL)
    return NULL;

  if (buf->buffername == NULL || buf->buffername[0] == '\0') {
    buf->buffername = checkHeader(buf, "Subject:");
    if (buf->buffername == NULL && buf->filename != NULL)
      buf->buffername = lastFileName(buf->filename);
  }
  if (buf->currentURL.scheme == SCM_UNKNOWN)
    buf->currentURL.scheme = f->scheme;
  if (f->scheme == SCM_LOCAL && buf->sourcefile == NULL)
    buf->sourcefile = buf->filename;
  if (loadproc == loadHTMLBuffer)
    buf->type = "text/html";
  else
    buf->type = "text/plain";
  return buf;
}

int dir_exist(char *path) {
  struct stat stbuf;

  if (path == NULL || *path == '\0')
    return 0;
  if (stat(path, &stbuf) == -1)
    return 0;
  return IS_DIRECTORY(stbuf.st_mode);
}

static int is_text_type(const char *type) {
  return (type == NULL || type[0] == '\0' ||
          strncasecmp(type, "text/", 5) == 0 ||
          (strncasecmp(type, "application/", 12) == 0 &&
           strstr(type, "xhtml") != NULL) ||
          strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int is_plain_text_type(const char *type) {
  return ((type && strcasecmp(type, "text/plain") == 0) ||
          (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(const char *type) {
  return (type && (strcasecmp(type, "text/html") == 0 ||
                   strcasecmp(type, "application/xhtml+xml") == 0));
}

static void check_compression(char *path, URLFile *uf) {
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
  Str *fn;
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

static int setModtime(char *path, time_t modtime) {
  struct utimbuf t;
  struct stat st;

  if (stat(path, &st) == 0)
    t.actime = st.st_atime;
  else
    t.actime = time(NULL);
  t.modtime = modtime;
  return utime(path, &t);
}

void examineFile(char *path, URLFile *uf) {
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

static int check_command(char *cmd, int auxbin_p) {
  static char *path = NULL;
  Str *dirs;
  char *p, *np;
  Str *pathname;
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

char *acceptableEncoding(void) {
  static Str *encodings = NULL;
  struct compression_decoder *d;
  TextList *l;
  char *p;

  if (encodings != NULL)
    return encodings->ptr;
  l = newTextList();
  for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++) {
    if (check_command(d->cmd, d->auxbin_p)) {
      pushText(l, d->encoding);
    }
  }
  encodings = Strnew();
  while ((p = popText(l)) != NULL) {
    if (encodings->length)
      Strcat_charp(encodings, ", ");
    Strcat_charp(encodings, p);
  }
  return encodings->ptr;
}

/*
 * convert line
 */
Str *convertLine0(URLFile *uf, Str *line, int mode) {
  if (mode != RAW_MODE)
    cleanup_line(line, mode);
  return line;
}

void readHeader(URLFile *uf, Buffer *newBuf, int thru, ParsedURL *pu) {
  char *p, *q;
  char *emsg;
  char c;
  Str *lineBuf2 = NULL;
  Str *tmp;
  TextList *headerlist;
  char *tmpf;
  FILE *src = NULL;
  Lineprop *propBuffer;

  headerlist = newBuf->document_header = newTextList();
  if (uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  if (thru && !newBuf->header_source) {
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    src = fopen(tmpf, "w");
    if (src)
      newBuf->header_source = tmpf;
  }
  while ((tmp = StrmyUFgets(uf)) && tmp->length) {
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
          addnewline(newBuf, lineBuf2->ptr, propBuffer, NULL, lineBuf2->length,
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

      process_http_cookie(pu, lineBuf2);

    } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
               uf->scheme == SCM_LOCAL_CGI) {
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
    if (headerlist)
      pushText(headerlist, lineBuf2->ptr);
    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
  if (thru)
    addnewline(newBuf, "", propBuffer, NULL, 0, -1, -1);
  if (src)
    fclose(src);
}

char *checkHeader(Buffer *buf, char *field) {
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

static char *checkContentType(Buffer *buf) {
  char *p;
  Str *r;
  p = checkHeader(buf, "Content-Type:");
  if (p == NULL)
    return NULL;
  r = Strnew();
  while (*p && *p != ';' && !IS_SPACE(*p))
    Strcat_char(r, *p++);
  return r->ptr;
}

struct auth_param {
  char *name;
  Str *val;
};

struct http_auth {
  int pri;
  char *scheme;
  struct auth_param *param;
  Str *(*cred)(struct http_auth *ha, Str *uname, Str *pw, ParsedURL *pu,
               HRequest *hr, FormList *request);
};

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

static Str *extract_auth_val(char **q) {
  unsigned char *qq = *(unsigned char **)q;
  int quoted = 0;
  Str *val = Strnew();

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

static Str *qstr_unquote(Str *s) {
  char *p;

  if (s == NULL)
    return NULL;
  p = s->ptr;
  if (*p == '"') {
    Str *tmp = Strnew();
    for (p++; *p != '\0'; p++) {
      if (*p == '\\')
        p++;
      Strcat_char(tmp, *p);
    }
    if (Strlastchar(tmp) == '"')
      Strshrink(tmp, 1);
    return tmp;
  } else
    return s;
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

static Str *get_auth_param(struct auth_param *auth, char *name) {
  struct auth_param *ap;
  for (ap = auth; ap->name != NULL; ap++) {
    if (strcasecmp(name, ap->name) == 0)
      return ap->val;
  }
  return NULL;
}

static Str *AuthBasicCred(struct http_auth *ha, Str *uname, Str *pw,
                          ParsedURL *pu, HRequest *hr, FormList *request) {
  Str *s = Strdup(uname);
  Strcat_char(s, ':');
  Strcat(s, pw);
  return Strnew_m_charp("Basic ", base64_encode(s->ptr, s->length)->ptr, NULL);
}

#ifdef USE_DIGEST_AUTH
#include <openssl/md5.h>

/* RFC2617: 3.2.2 The Authorization Request Header
 *
 * credentials      = "Digest" digest-response
 * digest-response  = 1#( username | realm | nonce | digest-uri
 *                    | response | [ algorithm ] | [cnonce] |
 *                     [opaque] | [message-qop] |
 *                         [nonce-count]  | [auth-param] )
 *
 * username         = "username" "=" username-value
 * username-value   = quoted-string
 * digest-uri       = "uri" "=" digest-uri-value
 * digest-uri-value = request-uri   ; As specified by HTTP/1.1
 * message-qop      = "qop" "=" qop-value
 * cnonce           = "cnonce" "=" cnonce-value
 * cnonce-value     = nonce-value
 * nonce-count      = "nc" "=" nc-value
 * nc-value         = 8LHEX
 * response         = "response" "=" request-digest
 * request-digest = <"> 32LHEX <">
 * LHEX             =  "0" | "1" | "2" | "3" |
 *                     "4" | "5" | "6" | "7" |
 *                     "8" | "9" | "a" | "b" |
 *                     "c" | "d" | "e" | "f"
 */

static Str *digest_hex(unsigned char *p) {
  char *h = "0123456789abcdef";
  Str *tmp = Strnew_size(MD5_DIGEST_LENGTH * 2 + 1);
  int i;
  for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++) {
    Strcat_char(tmp, h[(*p >> 4) & 0x0f]);
    Strcat_char(tmp, h[*p & 0x0f]);
  }
  return tmp;
}

enum {
  QOP_NONE,
  QOP_AUTH,
  QOP_AUTH_INT,
};

static Str *AuthDigestCred(struct http_auth *ha, Str *uname, Str *pw,
                           ParsedURL *pu, HRequest *hr, FormList *request) {
  Str *tmp, *a1buf, *a2buf, *rd, *s;
  unsigned char md5[MD5_DIGEST_LENGTH + 1];
  Str *uri = HTTPrequestURI(pu, hr);
  char nc[] = "00000001";
  FILE *fp;

  Str *algorithm = qstr_unquote(get_auth_param(ha->param, "algorithm"));
  Str *nonce = qstr_unquote(get_auth_param(ha->param, "nonce"));
  Str *cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
  /* cnonce is what client should generate. */
  Str *qop = qstr_unquote(get_auth_param(ha->param, "qop"));

  static union {
    int r[4];
    unsigned char s[sizeof(int) * 4];
  } cnonce_seed;
  int qop_i = QOP_NONE;

  cnonce_seed.r[0] = rand();
  cnonce_seed.r[1] = rand();
  cnonce_seed.r[2] = rand();
  MD5(cnonce_seed.s, sizeof(cnonce_seed.s), md5);
  cnonce = digest_hex(md5);
  cnonce_seed.r[3]++;

  if (qop) {
    char *p;
    size_t i;

    p = qop->ptr;
    SKIP_BLANKS(p);

    for (;;) {
      if ((i = strcspn(p, " \t,")) > 0) {
        if (i == sizeof("auth-int") - sizeof("") &&
            !strncasecmp(p, "auth-int", i)) {
          if (qop_i < QOP_AUTH_INT)
            qop_i = QOP_AUTH_INT;
        } else if (i == sizeof("auth") - sizeof("") &&
                   !strncasecmp(p, "auth", i)) {
          if (qop_i < QOP_AUTH)
            qop_i = QOP_AUTH;
        }
      }

      if (p[i]) {
        p += i + 1;
        SKIP_BLANKS(p);
      } else
        break;
    }
  }

  /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
  tmp = Strnew_m_charp(uname->ptr, ":",
                       qstr_unquote(get_auth_param(ha->param, "realm"))->ptr,
                       ":", pw->ptr, NULL);
  MD5((unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
  a1buf = digest_hex(md5);

  if (algorithm) {
    if (strcasecmp(algorithm->ptr, "MD5-sess") == 0) {
      /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
       *      ":" unq(nonce-value) ":" unq(cnonce-value)
       */
      if (nonce == NULL)
        return NULL;
      tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr, ":",
                           qstr_unquote(cnonce)->ptr, NULL);
      MD5((unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
      a1buf = digest_hex(md5);
    } else if (strcasecmp(algorithm->ptr, "MD5") == 0)
      /* ok default */
      ;
    else
      /* unknown algorithm */
      return NULL;
  }

  /* A2 = Method ":" digest-uri-value */
  tmp = Strnew_m_charp(HTTPrequestMethod(hr)->ptr, ":", uri->ptr, NULL);
  if (qop_i == QOP_AUTH_INT) {
    /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
    if (request && request->body) {
      if (request->method == FORM_METHOD_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        fp = fopen(request->body, "r");
        if (fp != NULL) {
          Str *ebody;
          ebody = Strfgetall(fp);
          fclose(fp);
          MD5((unsigned char *)ebody->ptr, strlen(ebody->ptr), md5);
        } else {
          MD5((unsigned char *)"", 0, md5);
        }
      } else {
        MD5((unsigned char *)request->body, request->length, md5);
      }
    } else {
      MD5((unsigned char *)"", 0, md5);
    }
    Strcat_char(tmp, ':');
    Strcat(tmp, digest_hex(md5));
  }
  MD5((unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
  a2buf = digest_hex(md5);

  if (qop_i >= QOP_AUTH) {
    /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
     *                      ":" nc-value
     *                      ":" unq(cnonce-value)
     *                      ":" unq(qop-value)
     *                      ":" H(A2)
     *                      ) <">
     */
    if (nonce == NULL)
      return NULL;
    tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr, ":", nc,
                         ":", qstr_unquote(cnonce)->ptr, ":",
                         qop_i == QOP_AUTH ? "auth" : "auth-int", ":",
                         a2buf->ptr, NULL);
    MD5((unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    rd = digest_hex(md5);
  } else {
    /* compatibility with RFC 2069
     * request_digest = KD(H(A1),  unq(nonce), H(A2))
     */
    tmp = Strnew_m_charp(a1buf->ptr, ":",
                         qstr_unquote(get_auth_param(ha->param, "nonce"))->ptr,
                         ":", a2buf->ptr, NULL);
    MD5((unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    rd = digest_hex(md5);
  }

  /*
   * digest-response  = 1#( username | realm | nonce | digest-uri
   *                          | response | [ algorithm ] | [cnonce] |
   *                          [opaque] | [message-qop] |
   *                          [nonce-count]  | [auth-param] )
   */

  tmp = Strnew_m_charp("Digest username=\"", uname->ptr, "\"", NULL);
  if ((s = get_auth_param(ha->param, "realm")) != NULL)
    Strcat_m_charp(tmp, ", realm=", s->ptr, NULL);
  if ((s = get_auth_param(ha->param, "nonce")) != NULL)
    Strcat_m_charp(tmp, ", nonce=", s->ptr, NULL);
  Strcat_m_charp(tmp, ", uri=\"", uri->ptr, "\"", NULL);
  Strcat_m_charp(tmp, ", response=\"", rd->ptr, "\"", NULL);

  if (algorithm && (s = get_auth_param(ha->param, "algorithm")))
    Strcat_m_charp(tmp, ", algorithm=", s->ptr, NULL);

  if (cnonce)
    Strcat_m_charp(tmp, ", cnonce=\"", cnonce->ptr, "\"", NULL);

  if ((s = get_auth_param(ha->param, "opaque")) != NULL)
    Strcat_m_charp(tmp, ", opaque=", s->ptr, NULL);

  if (qop_i >= QOP_AUTH) {
    Strcat_m_charp(tmp, ", qop=", qop_i == QOP_AUTH ? "auth" : "auth-int",
                   NULL);
    /* XXX how to count? */
    /* Since nonce is unique up to each *-Authenticate and w3m does not re-use
     *-Authenticate: headers, nonce-count should be always "00000001". */
    Strcat_m_charp(tmp, ", nc=", nc, NULL);
  }

  return tmp;
}
#endif

/* *INDENT-OFF* */
struct auth_param none_auth_param[] = {{NULL, NULL}};

struct auth_param basic_auth_param[] = {{"realm", NULL}, {NULL, NULL}};

#ifdef USE_DIGEST_AUTH
/* RFC2617: 3.2.1 The WWW-Authenticate Response Header
 * challenge        =  "Digest" digest-challenge
 *
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                       [ opaque ] |[ stale ] | [ algorithm ] |
 *                        [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * nonce             = "nonce" "=" nonce-value
 * nonce-value       = quoted-string
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                        token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */
struct auth_param digest_auth_param[] = {
    {"realm", NULL}, {"domain", NULL},    {"nonce", NULL}, {"opaque", NULL},
    {"stale", NULL}, {"algorithm", NULL}, {"qop", NULL},   {NULL, NULL}};
#endif
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
                                            Buffer *buf, char *auth_field) {
  struct http_auth *ha;
  int len = strlen(auth_field), slen;
  TextListItem *i;
  char *p0, *p;

  bzero(hauth, sizeof(struct http_auth));
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
                          TextList *extra_header, ParsedURL *pu, HRequest *hr,
                          FormList *request, Str **uname, Str **pwd) {
  Str *ss = NULL;
  Str *tmp;
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
    if (fmInitialized) {
      message("Wrong username or password", 0, 0);
      refresh(term_io());
    } else
      fprintf(stderr, "Wrong username or password\n");
    sleep(1);
    /* delete Authenticate: header from extra_header */
    delText(extra_header, i);
    invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
  }
  *uname = NULL;
  *pwd = NULL;

  if (!a_found &&
      find_auth_user_passwd(pu, realm, (Str **)uname, (Str **)pwd, proxy)) {
    /* found username & password in passwd file */;
  } else {
    if (IsForkChild)
      return;
    /* input username and password */
    sleep(2);
    if (fmInitialized) {
      char *pp;
      term_raw();
      if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr, NULL)) ==
          NULL)
        return;
      *uname = Strnew_charp(pp);
      if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
                          IN_PASSWORD)) == NULL) {
        *uname = NULL;
        return;
      }
      *pwd = Strnew_charp(pp);
      term_cbreak();
    } else {
      /*
       * If post file is specified as '-', stdin is closed at this
       * point.
       * In this case, w3m cannot read username from stdin.
       * So exit with error message.
       * (This is same behavior as lwp-request.)
       */
      if (feof(stdin) || ferror(stdin)) {
        /* FIXME: gettextize? */
        fprintf(stderr, "w3m: Authorization required for %s\n", realm);
        exit(1);
      }

      /* FIXME: gettextize? */
      printf(proxy ? "Proxy Username for %s: " : "Username for %s: ", realm);
      fflush(stdout);
      *uname = Strfgets(stdin);
      Strchop(*uname);
#ifdef HAVE_GETPASSPHRASE
      *pwd = Strnew_charp(
          (char *)getpassphrase(proxy ? "Proxy Password: " : "Password: "));
#else
      *pwd = Strnew_charp(
          (char *)getpass(proxy ? "Proxy Password: " : "Password: "));
#endif
    }
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

static int same_url_p(ParsedURL *pu1, ParsedURL *pu2) {
  return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
          (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1) &&
          (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

static int checkRedirection(ParsedURL *pu) {
  static ParsedURL *puv = NULL;
  static int nredir = 0;
  static int nredir_size = 0;
  Str *tmp;

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
    puv = (ParsedURL *)New_N(ParsedURL, nredir_size);
    memset(puv, 0, sizeof(ParsedURL) * nredir_size);
  }
  copyParsedURL(&puv[nredir % nredir_size], pu);
  nredir++;
  return TRUE;
}

static long long current_content_length;

/*
 * loadGeneralFile: load file to buffer
 */
#define DO_EXTERNAL ((Buffer * (*)(URLFile *, Buffer *)) doExternal)
Buffer *loadGeneralFile(const char *path, ParsedURL *current,
                        const char *referer, int flag, FormList *request) {
  URLFile f, *of = NULL;
  ParsedURL pu;
  Buffer *b = NULL;
  Buffer *(*proc)(URLFile *, Buffer *) = loadBuffer;
  const char *tpath;
  const char *t = "text/plain";
  char *p = NULL;
  const char *real_type = NULL;
  Buffer *t_buf = NULL;
  int searchHeader = SearchHeader;
  int searchHeader_through = TRUE;
  MySignalHandler prevtrap = NULL;
  TextList *extra_header = newTextList();
  Str *uname = NULL;
  Str *pwd = NULL;
  Str *realm = NULL;
  int add_auth_cookie_flag;
  unsigned char status = HTST_NORMAL;
  URLOption url_option;
  Str *tmp;
  Str *page = NULL;
  HRequest hr;
  ParsedURL *auth_pu;

  tpath = path;
  prevtrap = NULL;
  add_auth_cookie_flag = 0;

  checkRedirection(NULL);

load_doc: {
  const char *sc_redirect;
  parseURL2((char *)tpath, &pu, current);
  sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
  if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
    tpath = (char *)sc_redirect;
    request = NULL;
    add_auth_cookie_flag = 0;
    current = (ParsedURL *)New(ParsedURL);
    *current = pu;
    status = HTST_NORMAL;
    goto load_doc;
  }
}
  TRAP_OFF;
  url_option.referer = (char*)referer;
  url_option.flag = flag;
  f = openURL((char*)tpath, &pu, current, &url_option, request, extra_header, of, &hr,
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
          Str *cmd = Sprintf("%s?dir=%s#current", DirBufferCommand, pu.file);
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
      // page = loadFTPDir(&pu, &charset);
      // t = "ftp:directory";
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
      (((pu.scheme == SCM_FTP && non_null(FTP_proxy))) && use_proxy &&
       !check_no_proxy(pu.host))) {

    if (fmInitialized) {
      term_cbreak();
      /* FIXME: gettextize? */
      message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr, 0,
              0);
      refresh(term_io());
    }
    if (t_buf == NULL)
      t_buf = newBuffer(INIT_BUFFER_WIDTH);
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
      tpath = url_quote(p);
      request = NULL;
      UFclose(&f);
      current = (ParsedURL *)New(ParsedURL);
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
    check_compression((char*)path, &f);
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
      tpath = url_quote(remove_space(p));
      request = NULL;
      UFclose(&f);
      add_auth_cookie_flag = 0;
      current = (ParsedURL *)New(ParsedURL);
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
      Str *s;
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
      file = guess_save_name(NULL, pu.real_file);
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
    if ((is_text_type(t) || searchExtViewer(t))) {
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
  else if (w3m_backend)
    ;
  else if (is_dump_text_type(t)) {
    if (!do_download && searchExtViewer(t) != NULL) {
      proc = DO_EXTERNAL;
    } else {
      TRAP_OFF;
      if (pu.scheme == SCM_LOCAL) {
        UFclose(&f);
        _doFileCopy(pu.real_file, guess_save_name(NULL, pu.real_file), TRUE);
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
  t_buf->filename = pu.real_file ? pu.real_file : pu.file;
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
    if (w3m_backend)
      b->type = allocStr(t, -1);
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

extern Lineprop NullProp[];

/*
 * loadBuffer: read file and make new buffer
 */
Buffer *loadBuffer(URLFile *uf, Buffer *newBuf) {
  FILE *src = NULL;
  Str *lineBuf2;
  char pre_lbuf = '\0';
  int nlines;
  Str *tmpf;
  long long linelen = 0, trbyte = 0;
  Lineprop *propBuffer = NULL;
  MySignalHandler prevtrap = NULL;

  if (newBuf == NULL)
    newBuf = newBuffer(INIT_BUFFER_WIDTH);

  if (SETJMP(AbortLoading) != 0) {
    goto _end;
  }
  TRAP_ON;

  if (newBuf->sourcefile == NULL &&
      (uf->scheme != SCM_LOCAL || newBuf->mailcap)) {
    tmpf = tmpfname(TMPF_SRC, NULL);
    src = fopen(tmpf->ptr, "w");
    if (src)
      newBuf->sourcefile = tmpf->ptr;
  }

  nlines = 0;
  if (IStype(uf->stream) != IST_ENCODED)
    uf->stream = newEncodedStream(uf->stream, uf->encoding);
  while ((lineBuf2 = StrmyISgets(uf->stream)) && lineBuf2->length) {
    if (src)
      Strfputs(lineBuf2, src);
    linelen += lineBuf2->length;
    showProgress(&linelen, &trbyte, current_content_length);
    lineBuf2 = convertLine(uf, lineBuf2, PAGER_MODE, &charset, doc_charset);
    if (squeezeBlankLine) {
      if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        continue;
      }
      pre_lbuf = lineBuf2->ptr[0];
    }
    ++nlines;
    Strchop(lineBuf2);
    lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
    addnewline(newBuf, lineBuf2->ptr, propBuffer, colorBuffer, lineBuf2->length,
               FOLD_BUFFER_WIDTH, nlines);
  }
_end:
  TRAP_OFF;
  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  newBuf->trbyte = trbyte + linelen;
  if (src)
    fclose(src);

  return newBuf;
}

static Str *conv_symbol(Line *l) {
  Str *tmp = NULL;
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
static void _saveBuffer(Buffer *buf, Line *l, FILE *f, int cont) {
  Str *tmp;
  int is_html = FALSE;

  is_html = is_html_type(buf->type);

pager_next:
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
  if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE)) {
    l = getNextPage(buf, PagerMax);
    goto pager_next;
  }
}

void saveBuffer(Buffer *buf, FILE *f, int cont) {
  _saveBuffer(buf, buf->firstLine, f, cont);
}

void saveBufferBody(Buffer *buf, FILE *f, int cont) {
  Line *l = buf->firstLine;

  while (l != NULL && l->real_linenumber == 0)
    l = l->next;
  _saveBuffer(buf, l, f, cont);
}

static Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(URLFile *, Buffer *),
                          Buffer *defaultbuf) {
  FILE *f, *popen(const char *, const char *);
  Buffer *buf;
  URLFile uf;

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
Buffer *getshell(char *cmd) {
  Buffer *buf;

  buf = loadcmdout(cmd, loadBuffer, NULL);
  if (buf == NULL)
    return NULL;
  buf->filename = cmd;
  buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

/*
 * getpipe: execute shell command and connect pipe to the buffer
 */
Buffer *getpipe(char *cmd) {
  FILE *f, *popen(const char *, const char *);
  Buffer *buf;

  if (cmd == NULL || *cmd == '\0')
    return NULL;
  f = popen(cmd, "r");
  if (f == NULL)
    return NULL;
  buf = newBuffer(INIT_BUFFER_WIDTH);
  buf->pagerSource = newFileStream(f, (void (*)())pclose);
  buf->filename = cmd;
  buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME, cmd)->ptr;
  buf->bufferprop |= BP_PIPE;
  return buf;
}

/*
 * Open pager buffer
 */
Buffer *openPagerBuffer(input_stream *stream, Buffer *buf) {

  if (buf == NULL)
    buf = newBuffer(INIT_BUFFER_WIDTH);
  buf->pagerSource = stream;
  buf->buffername = getenv("MAN_PN");
  if (buf->buffername == NULL)
    buf->buffername = PIPEBUFFERNAME;
  buf->bufferprop |= BP_PIPE;
  buf->currentLine = buf->firstLine;

  return buf;
}

Buffer *openGeneralPagerBuffer(input_stream *stream) {
  Buffer *buf;
  const char *t = "text/plain";
  Buffer *t_buf = NULL;
  URLFile uf;

  init_stream(&uf, SCM_UNKNOWN, stream);

  t_buf = newBuffer(INIT_BUFFER_WIDTH);
  copyParsedURL(&t_buf->currentURL, NULL);
  t_buf->currentURL.scheme = SCM_LOCAL;
  t_buf->currentURL.file = "-";
  if (SearchHeader) {
    readHeader(&uf, t_buf, TRUE, NULL);
    t = checkContentType(t_buf);
    if (t == NULL)
      t = "text/plain";
    if (t_buf) {
      t_buf->topLine = t_buf->firstLine;
      t_buf->currentLine = t_buf->lastLine;
    }
    SearchHeader = FALSE;
  } else if (DefaultType) {
    t = DefaultType;
    DefaultType = NULL;
  }
  if (is_html_type(t)) {
    buf = loadHTMLBuffer(&uf, t_buf);
    buf->type = "text/html";
  } else if (is_plain_text_type(t)) {
    if (IStype(stream) != IST_ENCODED)
      stream = newEncodedStream(stream, uf.encoding);
    buf = openPagerBuffer(stream, t_buf);
    buf->type = "text/plain";
  } else {
    if (searchExtViewer(t)) {
      buf = doExternal(uf, t, t_buf);
      UFclose(&uf);
      if (buf == NULL || buf == NO_BUFFER)
        return buf;
    } else { /* unknown type is regarded as text/plain */
      if (IStype(stream) != IST_ENCODED)
        stream = newEncodedStream(stream, uf.encoding);
      buf = openPagerBuffer(stream, t_buf);
      buf->type = "text/plain";
    }
  }
  buf->real_type = t;
  return buf;
}

Line *getNextPage(Buffer *buf, int plen) {
  Line *top = buf->topLine, *last = buf->lastLine, *cur = buf->currentLine;
  int i;
  int nlines = 0;
  long long linelen = 0, trbyte = buf->trbyte;
  Str *lineBuf2;
  char pre_lbuf = '\0';
  URLFile uf;
  int squeeze_flag = FALSE;
  Lineprop *propBuffer = NULL;

  MySignalHandler prevtrap = NULL;

  if (buf->pagerSource == NULL)
    return NULL;

  if (last != NULL) {
    nlines = last->real_linenumber;
    pre_lbuf = *(last->lineBuf);
    if (pre_lbuf == '\0')
      pre_lbuf = '\n';
    buf->currentLine = last;
  }

  if (SETJMP(AbortLoading) != 0) {
    goto pager_end;
  }
  TRAP_ON;

  init_stream(&uf, SCM_UNKNOWN, NULL);
  for (i = 0; i < plen; i++) {
    if (!(lineBuf2 = StrmyISgets(buf->pagerSource)))
      return NULL;
    if (lineBuf2->length == 0) {
      /* Assume that `cmd == buf->filename' */
      if (buf->filename)
        buf->buffername = Sprintf("%s %s", CPIPEBUFFERNAME, buf->filename)->ptr;
      else if (getenv("MAN_PN") == NULL)
        buf->buffername = CPIPEBUFFERNAME;
      buf->bufferprop |= BP_CLOSE;
      break;
    }
    linelen += lineBuf2->length;
    showProgress(&linelen, &trbyte, current_content_length);
    lineBuf2 = convertLine(&uf, lineBuf2, PAGER_MODE, &charset, doc_charset);
    if (squeezeBlankLine) {
      squeeze_flag = FALSE;
      if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        --i;
        squeeze_flag = TRUE;
        continue;
      }
      pre_lbuf = lineBuf2->ptr[0];
    }
    ++nlines;
    Strchop(lineBuf2);
    lineBuf2 = checkType(lineBuf2, &propBuffer, &colorBuffer);
    addnewline(buf, lineBuf2->ptr, propBuffer, colorBuffer, lineBuf2->length,
               FOLD_BUFFER_WIDTH, nlines);
    if (!top) {
      top = buf->firstLine;
      cur = top;
    }
    if (buf->lastLine->real_linenumber - buf->firstLine->real_linenumber >=
        PagerMax) {
      Line *l = buf->firstLine;
      do {
        if (top == l)
          top = l->next;
        if (cur == l)
          cur = l->next;
        if (last == l)
          last = NULL;
        l = l->next;
      } while (l && l->bpos);
      buf->firstLine = l;
      if (l)
        buf->firstLine->prev = NULL;
    }
  }
pager_end:
  TRAP_OFF;

  buf->trbyte = trbyte + linelen;
  buf->topLine = top;
  buf->currentLine = cur;
  if (!last)
    last = buf->firstLine;
  else if (last && (last->next || !squeeze_flag))
    last = last->next;
  return last;
}

int save2tmp(URLFile uf, char *tmpf) {
  FILE *ff;
  long long linelen = 0, trbyte = 0;
  MySignalHandler prevtrap = NULL;
  static JMP_BUF env_bak;
  int retval = 0;
  char *buf = NULL;

  ff = fopen(tmpf, "wb");
  if (ff == NULL) {
    /* fclose(f); */
    return -1;
  }
  bcopy(AbortLoading, env_bak, sizeof(JMP_BUF));
  if (SETJMP(AbortLoading) != 0) {
    goto _end;
  }
  TRAP_ON;
  {
    int count;

    buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
    while ((count = ISread_n(uf.stream, buf, SAVE_BUF_SIZE)) > 0) {
      if (fwrite(buf, 1, count, ff) != count) {
        retval = -2;
        goto _end;
      }
      linelen += count;
      showProgress(&linelen, &trbyte, current_content_length);
    }
  }
_end:
  bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
  TRAP_OFF;
  xfree(buf);
  fclose(ff);
  current_content_length = 0;
  return retval;
}

Buffer *doExternal(URLFile uf, const char *type, Buffer *defaultbuf) {
  Str *tmpf, *command;
  struct mailcap *mcap;
  int mc_stat;
  Buffer *buf = NULL;
  char *header, *src = NULL, *ext = uf.ext;

  if (!(mcap = searchExtViewer(type)))
    return NULL;

  if (mcap->nametemplate) {
    tmpf = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
    if (tmpf->ptr[0] == '.')
      ext = tmpf->ptr;
  }
  tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);

  if (IStype(uf.stream) != IST_ENCODED)
    uf.stream = newEncodedStream(uf.stream, uf.encoding);
  header = checkHeader(defaultbuf, "Content-Type:");
  command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
  if (!(mc_stat & MCSTAT_REPNAME)) {
    Str *tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
    command = tmp;
  }

#ifdef HAVE_SETPGRP
  if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) &&
      !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer) {
    flush_tty();
    if (!fork()) {
      setup_child(FALSE, 0, UFfileno(&uf));
      if (save2tmp(uf, tmpf->ptr) < 0)
        exit(1);
      UFclose(&uf);
      myExec(command->ptr);
    }
    return NO_BUFFER;
  } else
#endif
  {
    if (save2tmp(uf, tmpf->ptr) < 0) {
      return NULL;
    }
  }
  if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
    if (defaultbuf == NULL)
      defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
    if (defaultbuf->sourcefile)
      src = defaultbuf->sourcefile;
    else
      src = tmpf->ptr;
    defaultbuf->sourcefile = NULL;
    defaultbuf->mailcap = mcap;
  }
  if (mcap->flags & MAILCAP_HTMLOUTPUT) {
    buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->type = "text/html";
      buf->mailcap_source = buf->sourcefile;
      buf->sourcefile = src;
    }
  } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
    buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->type = "text/plain";
      buf->mailcap_source = buf->sourcefile;
      buf->sourcefile = src;
    }
  } else {
    if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer) {
      fmTerm();
      mySystem(command->ptr, 0);
      fmInit();
      if (CurrentTab && Currentbuf)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
    } else {
      mySystem(command->ptr, 1);
    }
    buf = NO_BUFFER;
  }
  if (buf && buf != NO_BUFFER) {
    if ((buf->buffername == NULL || buf->buffername[0] == '\0') &&
        buf->filename)
      buf->buffername = lastFileName(buf->filename);
    buf->edit = mcap->edit;
    buf->mailcap = mcap;
  }
  return buf;
}

static int _MoveFile(char *path1, char *path2) {
  input_stream *f1;
  FILE *f2;
  int is_pipe;
  long long linelen = 0, trbyte = 0;
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
    showProgress(&linelen, &trbyte, current_content_length);
  }
  xfree(buf);
  ISclose(f1);
  if (is_pipe)
    pclose(f2);
  else
    fclose(f2);
  return 0;
}

int _doFileCopy(char *tmpf, char *defstr, int download) {
  Str *msg;
  Str *filen;
  char *p, *q = NULL;
  pid_t pid;
  char *lock;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
  FILE *f;
#endif
  struct stat st;
  long long size = 0;
  int is_pipe = FALSE;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      /* FIXME: gettextize? */
      q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
                        SaveHist);
      if (q == NULL || *q == '\0')
        return FALSE;
      p = q;
    }
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = TRUE;
    else {
      if (q) {
        p = unescape_spaces(Strnew_charp(q))->ptr;
      }
      p = expandPath(p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      msg = Sprintf("Can't copy. %s and %s are identical.", tmpf, p);
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        /* FIXME: gettextize? */
        msg = Sprintf("Can't save to %s", p);
        disp_err_message(msg->ptr, FALSE);
      }
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
    symlink(p, lock);
#else
    f = fopen(lock, "w");
    if (f)
      fclose(f);
#endif
    flush_tty();
    pid = fork();
    if (!pid) {
      setup_child(FALSE, 0, -1);
      if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe &&
          !stat(tmpf, &st))
        setModtime(p, st.st_mtime);
      unlink(lock);
      exit(0);
    }
    if (!stat(tmpf, &st))
      size = st.st_size;
    addDownloadList(pid, tmpf, p, lock, size);
  } else {
    q = searchKeyData();
    if (q == NULL || *q == '\0') {
      /* FIXME: gettextize? */
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = q;
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = TRUE;
    else {
      p = expandPath(p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't copy. %s and %s are identical.", tmpf, p);
      return -1;
    }
    if (_MoveFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
      setModtime(p, st.st_mtime);
  }
  return 0;
}

int doFileMove(char *tmpf, char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

int doFileSave(URLFile uf, char *defstr) {
  Str *msg;
  Str *filen;
  char *p, *q;
  pid_t pid;
  char *lock;
  char *tmpf = NULL;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
  FILE *f;
#endif

  if (fmInitialized) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      /* FIXME: gettextize? */
      p = inputLineHist("(Download)Save file to: ", defstr, IN_FILENAME,
                        SaveHist);
      if (p == NULL || *p == '\0')
        return -1;
    }
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(uf.stream, p) < 0) {
      /* FIXME: gettextize? */
      msg = Sprintf("Can't save. Load file and %s are identical.", p);
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
    symlink(p, lock);
#else
    f = fopen(lock, "w");
    if (f)
      fclose(f);
#endif
    flush_tty();
    pid = fork();
    if (!pid) {
      int err;
      if ((uf.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&uf, &tmpf);
        if (tmpf)
          unlink(tmpf);
      }
      setup_child(FALSE, 0, UFfileno(&uf));
      err = save2tmp(uf, p);
      if (err == 0 && PreserveTimestamp && uf.modtime != -1)
        setModtime(p, uf.modtime);
      UFclose(&uf);
      unlink(lock);
      if (err != 0)
        exit(-err);
      exit(0);
    }
    addDownloadList(pid, uf.url, p, lock, current_content_length);
  } else {
    q = searchKeyData();
    if (q == NULL || *q == '\0') {
      /* FIXME: gettextize? */
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = expandPath(q);
    if (!couldWrite(p))
      return -1;
    if (checkSaveFile(uf.stream, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save. Load file and %s are identical.", p);
      return -1;
    }
    if (uf.content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
      uncompress_stream(&uf, &tmpf);
      if (tmpf)
        unlink(tmpf);
    }
    if (save2tmp(uf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && uf.modtime != -1)
      setModtime(p, uf.modtime);
  }
  return 0;
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

int checkSaveFile(input_stream *stream, char *path2) {
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

bool couldWrite(const char *path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    // not exists
    return true;
  }

  // auto ans = inputAnswer("File exists. Overwrite? (y/n)");
  // if (ans && TOLOWER(*ans) == 'y'){
  //   return true;
  // }

  return false;
}

static void uncompress_stream(URLFile *uf, char **src) {
  pid_t pid1;
  FILE *f1;
  const char *expand_cmd = GUNZIP_CMDNAME;
  const char *expand_name = GUNZIP_NAME;
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
  Str *tmpf;
  int c, n = 0;

  lessopen = getenv("LESSOPEN");
  if (lessopen == NULL || lessopen[0] == '\0')
    return NULL;

  if (lessopen[0] != '|') /* filename mode, not supported m(__)m */
    return NULL;

  /* pipe mode */
  ++lessopen;

  /* LESSOPEN must contain one conversion specifier for strings ('%s'). */
  for (const char *f = lessopen; *f; f++) {
    if (*f == '%') {
      if (f[1] == '%') /* Literal % */
        f++;
      else if (*++f == 's') {
        if (n)
          return NULL;
        n++;
      } else
        return NULL;
    }
  }
  if (!n)
    return NULL;

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

char *guess_save_name(Buffer *buf, char *path) {
  if (buf && buf->document_header) {
    Str *name = NULL;
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

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length) {
  int i, j, rate, duration, eta, pos;
  static time_t last_time, start_time;
  time_t cur_time;
  Str *messages;
  char *fmtrbyte, *fmrate;

  if (!fmInitialized)
    return;

  if (*linelen < 1024)
    return;
  if (current_content_length > 0) {
    double ratio;
    cur_time = time(0);
    if (*trbyte == 0) {
      move(LASTLINE, 0);
      clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    move(LASTLINE, 0);
    ratio = 100.0 * (*trbyte) / current_content_length;
    fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
    duration = cur_time - start_time;
    if (duration) {
      rate = *trbyte / duration;
      fmrate = convert_size(rate, 1);
      eta = rate ? (current_content_length - *trbyte) / rate : -1;
      messages = Sprintf("%11s %3.0f%% "
                         "%7s/s "
                         "eta %02d:%02d:%02d     ",
                         fmtrbyte, ratio, fmrate, eta / (60 * 60),
                         (eta / 60) % 60, eta % 60);
    } else {
      messages =
          Sprintf("%11s %3.0f%%                          ", fmtrbyte, ratio);
    }
    addstr(messages->ptr);
    pos = 42;
    i = pos + (COLS - pos - 1) * (*trbyte) / current_content_length;
    move(LASTLINE, pos);
    standout();
    addch(' ');
    for (j = pos + 1; j <= i; j++)
      addch('|');
    standend();
    /* no_clrtoeol(); */
    refresh(term_io());
  } else {
    cur_time = time(0);
    if (*trbyte == 0) {
      move(LASTLINE, 0);
      clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    move(LASTLINE, 0);
    fmtrbyte = convert_size(*trbyte, 1);
    duration = cur_time - start_time;
    if (duration) {
      fmrate = convert_size(*trbyte / duration, 1);
      messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
    } else {
      messages = Sprintf("%7s loaded", fmtrbyte);
    }
    message(messages->ptr, 0, 0);
    refresh(term_io());
  }
}
