#include "file.h"
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

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

#define MAX_INPUT_SIZE 80 /* TODO - max should be screen line length */

static int frame_source = 0;

static char *guess_filename(char *file);
static FILE *lessopen_stream(char *path);
#define addnewline(a, b, c, d, e, f, g) _addnewline(a, b, c, e, f, g)
static void addnewline(struct Buffer *buf, char *line, Lineprop *prop,
                       Linecolor *color, int pos, int width, int nlines);
static void addLink(struct Buffer *buf, struct parsed_tag *tag);

static JMP_BUF AbortLoading;

static struct table *tables[MAX_TABLE];
static struct table_mode table_mode[MAX_TABLE];

#if defined(USE_M17N) || defined(USE_IMAGE)
static struct Url *cur_baseURL = NULL;
#endif

static Str cur_title;
static Str cur_select;
static Str select_str;
static int select_is_multiple;
static int n_selectitem;
static Str cur_option;
static Str cur_option_value;
static Str cur_option_label;
static int cur_option_selected;
static int cur_status;

static Str cur_textarea;
Str *textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
static int n_textarea;
static int ignore_nl_textarea;
int max_textarea = MAX_TEXTAREA;

static int http_response_code;

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct link_stack {
  int cmd;
  short offset;
  short pos;
  struct link_stack *next;
};

static struct link_stack *link_stack = NULL;

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#define INITIAL_FORM_SIZE 10
static struct FormList **forms;
static int *form_stack;
static int form_max = -1;
static int forms_size = 0;
#define cur_form_id ((form_sp >= 0) ? form_stack[form_sp] : -1)
static int form_sp = 0;

static int64_t current_content_length;

static int cur_hseq;

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)
#define IMG_SYMBOL UL_SYMBOL(12)
#define HR_SYMBOL 26

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

static MySignalHandler KeyAbort(SIGNAL_ARG) { LONGJMP(AbortLoading, 1); }

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

static struct Buffer *loadSomething(struct URLFile *f,
                                    struct Buffer *(*loadproc)(struct URLFile *,
                                                               struct Buffer *),
                                    struct Buffer *defaultbuf) {
  struct Buffer *buf;

  if ((buf = loadproc(f, defaultbuf)) == NULL)
    return NULL;

  if (buf->buffername == NULL || buf->buffername[0] == '\0') {
    buf->buffername = checkHeader(buf, "Subject:");
    if (buf->buffername == NULL && buf->filename != NULL)
      buf->buffername = conv_from_system(lastFileName(buf->filename));
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
  static Str encodings = NULL;
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
  char *p, *q;
  char *emsg;
  char c;
  Str lineBuf2 = NULL;
  Str tmp;
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
    addnewline(newBuf, "", propBuffer, NULL, 0, -1, -1);
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
                          TextList *extra_header, struct Url *pu,
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
    sleep(1);
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
    sleep(2);

    if (!term_inputAuth(realm, proxy, uname, pwd)) {
      return;
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

Str getLinkNumberStr(int correction) {
  return Sprintf("[%d]", cur_hseq + correction);
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
  TextList *extra_header = newTextList();
  Str uname = NULL;
  Str pwd = NULL;
  Str realm = NULL;
  int add_auth_cookie_flag;
  unsigned char status = HTST_NORMAL;
  URLOption url_option;
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
  if (flag & RG_FRAME) {
    t_buf->bufferprop |= BP_FRAME;
  }
  t_buf->ssl_certificate = f.ssl_certificate;
  frame_source = flag & RG_FRAME_SRC;
  if (proc == DO_EXTERNAL) {
    b = doExternal(f, t, t_buf);
  } else {
    b = loadSomething(&f, proc, t_buf);
  }
  UFclose(&f);
  frame_source = 0;
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

static char *has_hidden_link(struct readbuffer *obuf, int cmd) {
  Str line = obuf->line;
  struct link_stack *p;

  if (Strlastchar(line) != '>')
    return NULL;

  for (p = link_stack; p; p = p->next)
    if (p->cmd == cmd)
      break;
  if (!p)
    return NULL;

  if (obuf->pos == p->pos)
    return line->ptr + p->offset;

  return NULL;
}

static void push_link(int cmd, int offset, int pos) {
  struct link_stack *p;
  p = New(struct link_stack);
  p->cmd = cmd;
  p->offset = (short)offset;
  if (p->offset < 0)
    p->offset = 0;
  p->pos = (short)pos;
  if (p->pos < 0)
    p->pos = 0;
  p->next = link_stack;
  link_stack = p;
}

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

static void set_breakpoint(struct readbuffer *obuf, int tag_length) {
  obuf->bp.len = obuf->line->length;
  obuf->bp.pos = obuf->pos;
  obuf->bp.tlen = tag_length;
  obuf->bp.flag = obuf->flag;
  obuf->bp.flag &= ~RB_FILL;
  obuf->bp.top_margin = obuf->top_margin;
  obuf->bp.bottom_margin = obuf->bottom_margin;

  if (!obuf->bp.init_flag)
    return;

  memcpy((void *)&obuf->bp.anchor, (const void *)&obuf->anchor,
         sizeof(obuf->anchor));
  obuf->bp.img_alt = obuf->img_alt;
  obuf->bp.input_alt = obuf->input_alt;
  obuf->bp.in_bold = obuf->in_bold;
  obuf->bp.in_italic = obuf->in_italic;
  obuf->bp.in_under = obuf->in_under;
  obuf->bp.in_strike = obuf->in_strike;
  obuf->bp.in_ins = obuf->in_ins;
  obuf->bp.nobr_level = obuf->nobr_level;
  obuf->bp.prev_ctype = obuf->prev_ctype;
  obuf->bp.init_flag = 0;
}

static void back_to_breakpoint(struct readbuffer *obuf) {
  obuf->flag = obuf->bp.flag;
  memcpy((void *)&obuf->anchor, (const void *)&obuf->bp.anchor,
         sizeof(obuf->anchor));
  obuf->img_alt = obuf->bp.img_alt;
  obuf->input_alt = obuf->bp.input_alt;
  obuf->in_bold = obuf->bp.in_bold;
  obuf->in_italic = obuf->bp.in_italic;
  obuf->in_under = obuf->bp.in_under;
  obuf->in_strike = obuf->bp.in_strike;
  obuf->in_ins = obuf->bp.in_ins;
  obuf->prev_ctype = obuf->bp.prev_ctype;
  obuf->pos = obuf->bp.pos;
  obuf->top_margin = obuf->bp.top_margin;
  obuf->bottom_margin = obuf->bp.bottom_margin;
  if (obuf->flag & RB_NOBR)
    obuf->nobr_level = obuf->bp.nobr_level;
}

static void append_tags(struct readbuffer *obuf) {
  int i;
  int len = obuf->line->length;
  int set_bp = 0;

  for (i = 0; i < obuf->tag_sp; i++) {
    switch (obuf->tag_stack[i]->cmd) {
    case HTML_A:
    case HTML_IMG_ALT:
    case HTML_B:
    case HTML_U:
    case HTML_I:
    case HTML_S:
      push_link(obuf->tag_stack[i]->cmd, obuf->line->length, obuf->pos);
      break;
    }
    Strcat_charp(obuf->line, obuf->tag_stack[i]->cmdname);
    switch (obuf->tag_stack[i]->cmd) {
    case HTML_NOBR:
      if (obuf->nobr_level > 1)
        break;
    case HTML_WBR:
      set_bp = 1;
      break;
    }
  }
  obuf->tag_sp = 0;
  if (set_bp)
    set_breakpoint(obuf, obuf->line->length - len);
}

static void push_tag(struct readbuffer *obuf, char *cmdname, int cmd) {
  obuf->tag_stack[obuf->tag_sp] = New(struct cmdtable);
  obuf->tag_stack[obuf->tag_sp]->cmdname = allocStr(cmdname, -1);
  obuf->tag_stack[obuf->tag_sp]->cmd = cmd;
  obuf->tag_sp++;
  if (obuf->tag_sp >= TAG_STACK_SIZE || obuf->flag & (RB_SPECIAL & ~RB_NOBR))
    append_tags(obuf);
}

static void push_nchars(struct readbuffer *obuf, int width, char *str, int len,
                        Lineprop mode) {
  append_tags(obuf);
  Strcat_charp_n(obuf->line, str, len);
  obuf->pos += width;
  if (width > 0) {
    set_prevchar(obuf->prevchar, str, len);
    obuf->prev_ctype = mode;
  }
  obuf->flag |= RB_NFLUSHED;
}

#define push_charp(obuf, width, str, mode)                                     \
  push_nchars(obuf, width, str, strlen(str), mode)

#define push_str(obuf, width, str, mode)                                       \
  push_nchars(obuf, width, str->ptr, str->length, mode)

static void check_breakpoint(struct readbuffer *obuf, int pre_mode, char *ch) {
  int tlen, len = obuf->line->length;

  append_tags(obuf);
  if (pre_mode)
    return;
  tlen = obuf->line->length - len;
  if (tlen > 0 ||
      is_boundary((unsigned char *)obuf->prevchar->ptr, (unsigned char *)ch))
    set_breakpoint(obuf, tlen);
}

static void push_char(struct readbuffer *obuf, int pre_mode, char ch) {
  check_breakpoint(obuf, pre_mode, &ch);
  Strcat_char(obuf->line, ch);
  obuf->pos++;
  set_prevchar(obuf->prevchar, &ch, 1);
  if (ch != ' ')
    obuf->prev_ctype = PC_ASCII;
  obuf->flag |= RB_NFLUSHED;
}

#define PUSH(c) push_char(obuf, obuf->flag &RB_SPECIAL, c)

static void push_spaces(struct readbuffer *obuf, int pre_mode, int width) {
  int i;

  if (width <= 0)
    return;
  check_breakpoint(obuf, pre_mode, " ");
  for (i = 0; i < width; i++)
    Strcat_char(obuf->line, ' ');
  obuf->pos += width;
  set_space_to_prevchar(obuf->prevchar);
  obuf->flag |= RB_NFLUSHED;
}

static void proc_mchar(struct readbuffer *obuf, int pre_mode, int width,
                       char **str, Lineprop mode) {
  check_breakpoint(obuf, pre_mode, *str);
  obuf->pos += width;
  Strcat_charp_n(obuf->line, *str, get_mclen(*str));
  if (width > 0) {
    set_prevchar(obuf->prevchar, *str, 1);
    if (**str != ' ')
      obuf->prev_ctype = mode;
  }
  (*str) += get_mclen(*str);
  obuf->flag |= RB_NFLUSHED;
}

void push_render_image(Str str, int width, int limit,
                       struct html_feed_environ *h_env) {
  struct readbuffer *obuf = h_env->obuf;
  int indent = h_env->envs[h_env->envc].indent;

  push_spaces(obuf, 1, (limit - width) / 2);
  push_str(obuf, width, str, PC_ASCII);
  push_spaces(obuf, 1, (limit - width + 1) / 2);
  if (width > 0)
    flushline(h_env, obuf, indent, 0, h_env->limit);
}

static int sloppy_parse_line(char **str) {
  if (**str == '<') {
    while (**str && **str != '>')
      (*str)++;
    if (**str == '>')
      (*str)++;
    return 1;
  } else {
    while (**str && **str != '<')
      (*str)++;
    return 0;
  }
}

static void passthrough(struct readbuffer *obuf, char *str, int back) {
  int cmd;
  Str tok = Strnew();
  char *str_bak;

  if (back) {
    Str str_save = Strnew_charp(str);
    Strshrink(obuf->line, obuf->line->ptr + obuf->line->length - str);
    str = str_save->ptr;
  }
  while (*str) {
    str_bak = str;
    if (sloppy_parse_line(&str)) {
      char *q = str_bak;
      cmd = gethtmlcmd(&q);
      if (back) {
        struct link_stack *p;
        for (p = link_stack; p; p = p->next) {
          if (p->cmd == cmd) {
            link_stack = p->next;
            break;
          }
        }
        back = 0;
      } else {
        Strcat_charp_n(tok, str_bak, str - str_bak);
        push_tag(obuf, tok->ptr, cmd);
        Strclear(tok);
      }
    } else {
      push_nchars(obuf, 0, str_bak, str - str_bak, obuf->prev_ctype);
    }
  }
}

void fillline(struct readbuffer *obuf, int indent) {
  push_spaces(obuf, 1, indent - obuf->pos);
  obuf->flag &= ~RB_NFLUSHED;
}

void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
               int indent, int force, int width) {
  TextLineList *buf = h_env->buf;
  FILE *f = h_env->f;
  Str line = obuf->line, pass = NULL;
  char *hidden_anchor = NULL, *hidden_img = NULL, *hidden_bold = NULL,
       *hidden_under = NULL, *hidden_italic = NULL, *hidden_strike = NULL,
       *hidden_ins = NULL, *hidden_input = NULL, *hidden = NULL;

#ifdef DEBUG
  if (w3m_debug) {
    FILE *df = fopen("zzzproc1", "a");
    fprintf(df, "flushline(%s,%d,%d,%d)\n", obuf->line->ptr, indent, force,
            width);
    if (buf) {
      TextLineListItem *p;
      for (p = buf->first; p; p = p->next) {
        fprintf(df, "buf=\"%s\"\n", p->ptr->line->ptr);
      }
    }
    fclose(df);
  }
#endif

  if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && Strlastchar(line) == ' ') {
    Strshrink(line, 1);
    obuf->pos--;
  }

  append_tags(obuf);

  if (obuf->anchor.url)
    hidden = hidden_anchor = has_hidden_link(obuf, HTML_A);
  if (obuf->img_alt) {
    if ((hidden_img = has_hidden_link(obuf, HTML_IMG_ALT)) != NULL) {
      if (!hidden || hidden_img < hidden)
        hidden = hidden_img;
    }
  }
  if (obuf->input_alt.in) {
    if ((hidden_input = has_hidden_link(obuf, HTML_INPUT_ALT)) != NULL) {
      if (!hidden || hidden_input < hidden)
        hidden = hidden_input;
    }
  }
  if (obuf->in_bold) {
    if ((hidden_bold = has_hidden_link(obuf, HTML_B)) != NULL) {
      if (!hidden || hidden_bold < hidden)
        hidden = hidden_bold;
    }
  }
  if (obuf->in_italic) {
    if ((hidden_italic = has_hidden_link(obuf, HTML_I)) != NULL) {
      if (!hidden || hidden_italic < hidden)
        hidden = hidden_italic;
    }
  }
  if (obuf->in_under) {
    if ((hidden_under = has_hidden_link(obuf, HTML_U)) != NULL) {
      if (!hidden || hidden_under < hidden)
        hidden = hidden_under;
    }
  }
  if (obuf->in_strike) {
    if ((hidden_strike = has_hidden_link(obuf, HTML_S)) != NULL) {
      if (!hidden || hidden_strike < hidden)
        hidden = hidden_strike;
    }
  }
  if (obuf->in_ins) {
    if ((hidden_ins = has_hidden_link(obuf, HTML_INS)) != NULL) {
      if (!hidden || hidden_ins < hidden)
        hidden = hidden_ins;
    }
  }
  if (hidden) {
    pass = Strnew_charp(hidden);
    Strshrink(line, line->ptr + line->length - hidden);
  }

  if (!(obuf->flag & (RB_SPECIAL & ~RB_NOBR)) && obuf->pos > width) {
    char *tp = &line->ptr[obuf->bp.len - obuf->bp.tlen];
    char *ep = &line->ptr[line->length];

    if (obuf->bp.pos == obuf->pos && tp <= ep && tp > line->ptr &&
        tp[-1] == ' ') {
      memcpy(tp - 1, tp, ep - tp + 1);
      line->length--;
      obuf->pos--;
    }
  }

  if (obuf->anchor.url && !hidden_anchor)
    Strcat_charp(line, "</a>");
  if (obuf->img_alt && !hidden_img)
    Strcat_charp(line, "</img_alt>");
  if (obuf->input_alt.in && !hidden_input)
    Strcat_charp(line, "</input_alt>");
  if (obuf->in_bold && !hidden_bold)
    Strcat_charp(line, "</b>");
  if (obuf->in_italic && !hidden_italic)
    Strcat_charp(line, "</i>");
  if (obuf->in_under && !hidden_under)
    Strcat_charp(line, "</u>");
  if (obuf->in_strike && !hidden_strike)
    Strcat_charp(line, "</s>");
  if (obuf->in_ins && !hidden_ins)
    Strcat_charp(line, "</ins>");

  if (obuf->top_margin > 0) {
    int i;
    struct html_feed_environ h;
    struct readbuffer o;
    struct environment e[1];

    init_henv(&h, &o, e, 1, NULL, width, indent);
    o.line = Strnew_size(width + 20);
    o.pos = obuf->pos;
    o.flag = obuf->flag;
    o.top_margin = -1;
    o.bottom_margin = -1;
    Strcat_charp(o.line, "<pre_int>");
    for (i = 0; i < o.pos; i++)
      Strcat_char(o.line, ' ');
    Strcat_charp(o.line, "</pre_int>");
    for (i = 0; i < obuf->top_margin; i++)
      flushline(h_env, &o, indent, force, width);
  }

  if (force == 1 || obuf->flag & RB_NFLUSHED) {
    TextLine *lbuf = newTextLine(line, obuf->pos);
    if (RB_GET_ALIGN(obuf) == RB_CENTER) {
      align(lbuf, width, ALIGN_CENTER);
    } else if (RB_GET_ALIGN(obuf) == RB_RIGHT) {
      align(lbuf, width, ALIGN_RIGHT);
    } else if (RB_GET_ALIGN(obuf) == RB_LEFT && obuf->flag & RB_INTABLE) {
      align(lbuf, width, ALIGN_LEFT);
    } else if (obuf->flag & RB_FILL) {
      char *p;
      int rest, rrest;
      int nspace, d, i;

      rest = width - get_Str_strwidth(line);
      if (rest > 1) {
        nspace = 0;
        for (p = line->ptr + indent; *p; p++) {
          if (*p == ' ')
            nspace++;
        }
        if (nspace > 0) {
          int indent_here = 0;
          d = rest / nspace;
          p = line->ptr;
          while (IS_SPACE(*p)) {
            p++;
            indent_here++;
          }
          rrest = rest - d * nspace;
          line = Strnew_size(width + 1);
          for (i = 0; i < indent_here; i++)
            Strcat_char(line, ' ');
          for (; *p; p++) {
            Strcat_char(line, *p);
            if (*p == ' ') {
              for (i = 0; i < d; i++)
                Strcat_char(line, ' ');
              if (rrest > 0) {
                Strcat_char(line, ' ');
                rrest--;
              }
            }
          }
          lbuf = newTextLine(line, width);
        }
      }
    }
#ifdef TABLE_DEBUG
    if (w3m_debug) {
      FILE *f = fopen("zzzproc1", "a");
      fprintf(f, "pos=%d,%d, maxlimit=%d\n", visible_length(lbuf->line->ptr),
              lbuf->pos, h_env->maxlimit);
      fclose(f);
    }
#endif
    if (lbuf->pos > h_env->maxlimit)
      h_env->maxlimit = lbuf->pos;
    if (buf)
      pushTextLine(buf, lbuf);
    else if (f) {
      Strfputs(Str_conv_to_halfdump(lbuf->line), f);
      fputc('\n', f);
    }
    if (obuf->flag & RB_SPECIAL || obuf->flag & RB_NFLUSHED)
      h_env->blank_lines = 0;
    else
      h_env->blank_lines++;
  } else {
    char *p = line->ptr, *q;
    Str tmp = Strnew(), tmp2 = Strnew();

#define APPEND(str)                                                            \
  if (buf)                                                                     \
    appendTextLine(buf, (str), 0);                                             \
  else if (f)                                                                  \
  Strfputs((str), f)

    while (*p) {
      q = p;
      if (sloppy_parse_line(&p)) {
        Strcat_charp_n(tmp, q, p - q);
        if (force == 2) {
          APPEND(tmp);
        } else
          Strcat(tmp2, tmp);
        Strclear(tmp);
      }
    }
    if (force == 2) {
      if (pass) {
        APPEND(pass);
      }
      pass = NULL;
    } else {
      if (pass)
        Strcat(tmp2, pass);
      pass = tmp2;
    }
  }

  if (obuf->bottom_margin > 0) {
    int i;
    struct html_feed_environ h;
    struct readbuffer o;
    struct environment e[1];

    init_henv(&h, &o, e, 1, NULL, width, indent);
    o.line = Strnew_size(width + 20);
    o.pos = obuf->pos;
    o.flag = obuf->flag;
    o.top_margin = -1;
    o.bottom_margin = -1;
    Strcat_charp(o.line, "<pre_int>");
    for (i = 0; i < o.pos; i++)
      Strcat_char(o.line, ' ');
    Strcat_charp(o.line, "</pre_int>");
    for (i = 0; i < obuf->bottom_margin; i++)
      flushline(h_env, &o, indent, force, width);
  }
  if (obuf->top_margin < 0 || obuf->bottom_margin < 0)
    return;

  obuf->line = Strnew_size(256);
  obuf->pos = 0;
  obuf->top_margin = 0;
  obuf->bottom_margin = 0;
  set_space_to_prevchar(obuf->prevchar);
  obuf->bp.init_flag = 1;
  obuf->flag &= ~RB_NFLUSHED;
  set_breakpoint(obuf, 0);
  obuf->prev_ctype = PC_ASCII;
  link_stack = NULL;
  fillline(obuf, indent);
  if (pass)
    passthrough(obuf, pass->ptr, 0);
  if (!hidden_anchor && obuf->anchor.url) {
    Str tmp;
    if (obuf->anchor.hseq > 0)
      obuf->anchor.hseq = -obuf->anchor.hseq;
    tmp = Sprintf("<A HSEQ=\"%d\" HREF=\"", obuf->anchor.hseq);
    Strcat_charp(tmp, html_quote(obuf->anchor.url));
    if (obuf->anchor.target) {
      Strcat_charp(tmp, "\" TARGET=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.target));
    }
    if (obuf->anchor.referer) {
      Strcat_charp(tmp, "\" REFERER=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.referer));
    }
    if (obuf->anchor.title) {
      Strcat_charp(tmp, "\" TITLE=\"");
      Strcat_charp(tmp, html_quote(obuf->anchor.title));
    }
    if (obuf->anchor.accesskey) {
      char *c = html_quote_char(obuf->anchor.accesskey);
      Strcat_charp(tmp, "\" ACCESSKEY=\"");
      if (c)
        Strcat_charp(tmp, c);
      else
        Strcat_char(tmp, obuf->anchor.accesskey);
    }
    Strcat_charp(tmp, "\">");
    push_tag(obuf, tmp->ptr, HTML_A);
  }
  if (!hidden_img && obuf->img_alt) {
    Str tmp = Strnew_charp("<IMG_ALT SRC=\"");
    Strcat_charp(tmp, html_quote(obuf->img_alt->ptr));
    Strcat_charp(tmp, "\">");
    push_tag(obuf, tmp->ptr, HTML_IMG_ALT);
  }
  if (!hidden_input && obuf->input_alt.in) {
    Str tmp;
    if (obuf->input_alt.hseq > 0)
      obuf->input_alt.hseq = -obuf->input_alt.hseq;
    tmp = Sprintf("<INPUT_ALT hseq=\"%d\" fid=\"%d\" name=\"%s\" type=\"%s\" "
                  "value=\"%s\">",
                  obuf->input_alt.hseq, obuf->input_alt.fid,
                  obuf->input_alt.name ? obuf->input_alt.name->ptr : "",
                  obuf->input_alt.type ? obuf->input_alt.type->ptr : "",
                  obuf->input_alt.value ? obuf->input_alt.value->ptr : "");
    push_tag(obuf, tmp->ptr, HTML_INPUT_ALT);
  }
  if (!hidden_bold && obuf->in_bold)
    push_tag(obuf, "<B>", HTML_B);
  if (!hidden_italic && obuf->in_italic)
    push_tag(obuf, "<I>", HTML_I);
  if (!hidden_under && obuf->in_under)
    push_tag(obuf, "<U>", HTML_U);
  if (!hidden_strike && obuf->in_strike)
    push_tag(obuf, "<S>", HTML_S);
  if (!hidden_ins && obuf->in_ins)
    push_tag(obuf, "<INS>", HTML_INS);
}

void do_blankline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                  int indent, int indent_incr, int width) {
  if (h_env->blank_lines == 0)
    flushline(h_env, obuf, indent, 1, width);
}

void purgeline(struct html_feed_environ *h_env) {
  char *p, *q;
  Str tmp;

  if (h_env->buf == NULL || h_env->blank_lines == 0)
    return;

  p = rpopTextLine(h_env->buf)->line->ptr;
  tmp = Strnew();
  while (*p) {
    q = p;
    if (sloppy_parse_line(&p)) {
      Strcat_charp_n(tmp, q, p - q);
    }
  }
  appendTextLine(h_env->buf, tmp, 0);
  h_env->blank_lines--;
}

static int close_effect0(struct readbuffer *obuf, int cmd) {
  int i;
  char *p;

  for (i = obuf->tag_sp - 1; i >= 0; i--) {
    if (obuf->tag_stack[i]->cmd == cmd)
      break;
  }
  if (i >= 0) {
    obuf->tag_sp--;
    memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1],
           (obuf->tag_sp - i) * sizeof(struct cmdtable *));
    return 1;
  } else if ((p = has_hidden_link(obuf, cmd)) != NULL) {
    passthrough(obuf, p, 1);
    return 1;
  }
  return 0;
}

static void close_anchor(struct html_feed_environ *h_env,
                         struct readbuffer *obuf) {
  if (obuf->anchor.url) {
    int i;
    char *p = NULL;
    int is_erased = 0;

    for (i = obuf->tag_sp - 1; i >= 0; i--) {
      if (obuf->tag_stack[i]->cmd == HTML_A)
        break;
    }
    if (i < 0 && obuf->anchor.hseq > 0 && Strlastchar(obuf->line) == ' ') {
      Strshrink(obuf->line, 1);
      obuf->pos--;
      is_erased = 1;
    }

    if (i >= 0 || (p = has_hidden_link(obuf, HTML_A))) {
      if (obuf->anchor.hseq > 0) {
        HTMLlineproc1(ANSP, h_env);
        set_space_to_prevchar(obuf->prevchar);
      } else {
        if (i >= 0) {
          obuf->tag_sp--;
          memcpy(&obuf->tag_stack[i], &obuf->tag_stack[i + 1],
                 (obuf->tag_sp - i) * sizeof(struct cmdtable *));
        } else {
          passthrough(obuf, p, 1);
        }
        memset((void *)&obuf->anchor, 0, sizeof(obuf->anchor));
        return;
      }
      is_erased = 0;
    }
    if (is_erased) {
      Strcat_char(obuf->line, ' ');
      obuf->pos++;
    }

    push_tag(obuf, "</a>", HTML_N_A);
  }
  memset((void *)&obuf->anchor, 0, sizeof(obuf->anchor));
}

void save_fonteffect(struct html_feed_environ *h_env, struct readbuffer *obuf) {
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    memcpy(obuf->fontstat_stack[obuf->fontstat_sp], obuf->fontstat,
           FONTSTAT_SIZE);
  if (obuf->fontstat_sp < INT_MAX)
    obuf->fontstat_sp++;
  if (obuf->in_bold)
    push_tag(obuf, "</b>", HTML_N_B);
  if (obuf->in_italic)
    push_tag(obuf, "</i>", HTML_N_I);
  if (obuf->in_under)
    push_tag(obuf, "</u>", HTML_N_U);
  if (obuf->in_strike)
    push_tag(obuf, "</s>", HTML_N_S);
  if (obuf->in_ins)
    push_tag(obuf, "</ins>", HTML_N_INS);
  memset(obuf->fontstat, 0, FONTSTAT_SIZE);
}

void restore_fonteffect(struct html_feed_environ *h_env,
                        struct readbuffer *obuf) {
  if (obuf->fontstat_sp > 0)
    obuf->fontstat_sp--;
  if (obuf->fontstat_sp < FONT_STACK_SIZE)
    memcpy(obuf->fontstat, obuf->fontstat_stack[obuf->fontstat_sp],
           FONTSTAT_SIZE);
  if (obuf->in_bold)
    push_tag(obuf, "<b>", HTML_B);
  if (obuf->in_italic)
    push_tag(obuf, "<i>", HTML_I);
  if (obuf->in_under)
    push_tag(obuf, "<u>", HTML_U);
  if (obuf->in_strike)
    push_tag(obuf, "<s>", HTML_S);
  if (obuf->in_ins)
    push_tag(obuf, "<ins>", HTML_INS);
}

static Str process_title(struct parsed_tag *tag) {
  cur_title = Strnew();
  return NULL;
}

static Str process_n_title(struct parsed_tag *tag) {
  Str tmp;

  if (!cur_title)
    return NULL;
  Strremovefirstspaces(cur_title);
  Strremovetrailingspaces(cur_title);
  tmp = Strnew_m_charp("<title_alt title=\"", html_quote(cur_title->ptr), "\">",
                       NULL);
  cur_title = NULL;
  return tmp;
}

static void feed_title(char *str) {
  if (!cur_title)
    return;
  while (*str) {
    if (*str == '&')
      Strcat_charp(cur_title, getescapecmd(&str));
    else if (*str == '\n' || *str == '\r') {
      Strcat_char(cur_title, ' ');
      str++;
    } else
      Strcat_char(cur_title, *(str++));
  }
}

Str process_img(struct parsed_tag *tag, int width) {
  char *p, *q, *r, *r2 = NULL, *s, *t;
  int w, i, nw, n;
  int pre_int = FALSE, ext_pre_int = FALSE;
  Str tmp = Strnew();

  if (!parsedtag_get_value(tag, ATTR_SRC, &p))
    return tmp;
  p = url_encode(remove_space(p), cur_baseURL, cur_document_charset);
  q = NULL;
  parsedtag_get_value(tag, ATTR_ALT, &q);
  if (!pseudoInlines && (q == NULL || (*q == '\0' && ignore_null_img_alt)))
    return tmp;
  t = q;
  parsedtag_get_value(tag, ATTR_TITLE, &t);
  w = -1;
  if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
    if (w < 0) {
      if (width > 0)
        w = (int)(-width * pixel_per_char * w / 100 + 0.5);
      else
        w = -1;
    }
  }
  i = -1;
  parsedtag_get_value(tag, ATTR_HEIGHT, &i);
  r = NULL;
  parsedtag_get_value(tag, ATTR_USEMAP, &r);
  if (parsedtag_exists(tag, ATTR_PRE_INT))
    ext_pre_int = TRUE;

  tmp = Strnew_size(128);
  if (r) {
    Str tmp2;
    r2 = strchr(r, '#');
    s = "<form_int method=internal action=map>";
    tmp2 = process_form(parse_tag(&s, TRUE));
    if (tmp2)
      Strcat(tmp, tmp2);
    Strcat(tmp, Sprintf("<input_alt fid=\"%d\" "
                        "type=hidden name=link value=\"",
                        cur_form_id));
    Strcat_charp(tmp, html_quote((r2) ? r2 + 1 : r));
    Strcat(tmp, Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                        "type=submit no_effect=true>",
                        cur_hseq++, cur_form_id));
  }
  {
    if (w < 0)
      w = 12 * pixel_per_char;
    nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
    if (r) {
      Strcat_charp(tmp, "<pre_int>");
      pre_int = TRUE;
    }
    Strcat_charp(tmp, "<img_alt src=\"");
  }
  Strcat_charp(tmp, html_quote(p));
  Strcat_charp(tmp, "\"");
  if (t) {
    Strcat_charp(tmp, " title=\"");
    Strcat_charp(tmp, html_quote(t));
    Strcat_charp(tmp, "\"");
  }
  Strcat_charp(tmp, ">");
  if (q != NULL && *q == '\0' && ignore_null_img_alt)
    q = NULL;
  if (q != NULL) {
    n = get_strwidth(q);
    Strcat_charp(tmp, html_quote(q));
    goto img_end;
  }
  if (w > 0 && i > 0) {
    /* guess what the image is! */
    if (w < 32 && i < 48) {
      /* must be an icon or space */
      n = 1;
      if (strcasestr(p, "space") || strcasestr(p, "blank"))
        Strcat_charp(tmp, "_");
      else {
        if (w * i < 8 * 16)
          Strcat_charp(tmp, "*");
        else {
          if (!pre_int) {
            Strcat_charp(tmp, "<pre_int>");
            pre_int = TRUE;
          }
          push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
          n = symbol_width;
        }
      }
      goto img_end;
    }
    if (w > 200 && i < 13) {
      /* must be a horizontal line */
      if (!pre_int) {
        Strcat_charp(tmp, "<pre_int>");
        pre_int = TRUE;
      }
      w = w / pixel_per_char / symbol_width;
      if (w <= 0)
        w = 1;
      push_symbol(tmp, HR_SYMBOL, symbol_width, w);
      n = w * symbol_width;
      goto img_end;
    }
  }
  for (q = p; *q; q++)
    ;
  while (q > p && *q != '/')
    q--;
  if (*q == '/')
    q++;
  Strcat_char(tmp, '[');
  n = 1;
  p = q;
  for (; *q; q++) {
    if (!IS_ALNUM(*q) && *q != '_' && *q != '-') {
      break;
    }
    Strcat_char(tmp, *q);
    n++;
    if (n + 1 >= nw)
      break;
  }
  Strcat_char(tmp, ']');
  n++;
img_end:
  Strcat_charp(tmp, "</img_alt>");
  if (pre_int && !ext_pre_int)
    Strcat_charp(tmp, "</pre_int>");
  if (r) {
    Strcat_charp(tmp, "</input_alt>");
    process_n_form();
  }
  return tmp;
}

Str process_anchor(struct parsed_tag *tag, char *tagbuf) {
  if (parsedtag_need_reconstruct(tag)) {
    parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
    return parsedtag2str(tag);
  } else {
    Str tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
    Strcat_charp(tmp, tagbuf + 2);
    return tmp;
  }
}

Str process_input(struct parsed_tag *tag) {
  int i = 20, v, x, y, z, iw, ih, size = 20;
  char *q, *p, *r, *p2, *s;
  Str tmp = NULL;
  char *qq = "";
  int qlen = 0;

  if (cur_form_id < 0) {
    char *s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }
  if (tmp == NULL)
    tmp = Strnew();

  p = "text";
  parsedtag_get_value(tag, ATTR_TYPE, &p);
  q = NULL;
  parsedtag_get_value(tag, ATTR_VALUE, &q);
  r = "";
  parsedtag_get_value(tag, ATTR_NAME, &r);
  parsedtag_get_value(tag, ATTR_SIZE, &size);
  if (size > MAX_INPUT_SIZE)
    size = MAX_INPUT_SIZE;
  parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
  p2 = NULL;
  parsedtag_get_value(tag, ATTR_ALT, &p2);
  x = parsedtag_exists(tag, ATTR_CHECKED);
  y = parsedtag_exists(tag, ATTR_ACCEPT);
  z = parsedtag_exists(tag, ATTR_READONLY);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return NULL;

  if (!q) {
    switch (v) {
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
      /* if no VALUE attribute is specified in
       * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used
       * as a default value. It is not a part of HTML4.0
       * specification, but an imitation of Netscape behaviour.
       */
    case FORM_INPUT_CHECKBOX:
      q = "on";
    }
  }
  /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
  if (v == FORM_INPUT_FILE)
    q = NULL;
  if (q) {
    qq = html_quote(q);
    qlen = get_strwidth(q);
  }

  Strcat_charp(tmp, "<pre_int>");
  switch (v) {
  case FORM_INPUT_PASSWORD:
  case FORM_INPUT_TEXT:
  case FORM_INPUT_FILE:
  case FORM_INPUT_CHECKBOX:
    if (displayLinkNumber)
      Strcat(tmp, getLinkNumberStr(0));
    Strcat_char(tmp, '[');
    break;
  case FORM_INPUT_RADIO:
    if (displayLinkNumber)
      Strcat(tmp, getLinkNumberStr(0));
    Strcat_char(tmp, '(');
  }
  Strcat(tmp, Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      cur_hseq++, cur_form_id, html_quote(p), html_quote(r),
                      size, i, qq));
  if (x)
    Strcat_charp(tmp, " checked");
  if (y)
    Strcat_charp(tmp, " accept");
  if (z)
    Strcat_charp(tmp, " readonly");
  Strcat_char(tmp, '>');

  if (v == FORM_INPUT_HIDDEN)
    Strcat_charp(tmp, "</input_alt></pre_int>");
  else {
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "<u>");
      break;
    case FORM_INPUT_IMAGE:
      s = NULL;
      parsedtag_get_value(tag, ATTR_SRC, &s);
      if (s) {
        Strcat(tmp, Sprintf("<img src=\"%s\"", html_quote(s)));
        if (p2)
          Strcat(tmp, Sprintf(" alt=\"%s\"", html_quote(p2)));
        if (parsedtag_get_value(tag, ATTR_WIDTH, &iw))
          Strcat(tmp, Sprintf(" width=\"%d\"", iw));
        if (parsedtag_get_value(tag, ATTR_HEIGHT, &ih))
          Strcat(tmp, Sprintf(" height=\"%d\"", ih));
        Strcat_charp(tmp, " pre_int>");
        Strcat_charp(tmp, "</input_alt></pre_int>");
        return tmp;
      }
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      if (displayLinkNumber)
        Strcat(tmp, getLinkNumberStr(-1));
      Strcat_charp(tmp, "[");
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
      i = 0;
      if (q) {
        for (; i < qlen && i < size; i++)
          Strcat_char(tmp, '*');
      }
      for (; i < size; i++)
        Strcat_char(tmp, ' ');
      break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      if (q)
        Strcat(tmp, textfieldrep(Strnew_charp(q), size));
      else {
        for (i = 0; i < size; i++)
          Strcat_char(tmp, ' ');
      }
      break;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      if (p2)
        Strcat_charp(tmp, html_quote(p2));
      else
        Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, qq);
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (x)
        Strcat_char(tmp, '*');
      else
        Strcat_char(tmp, ' ');
      break;
    }
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
      Strcat_charp(tmp, "</u>");
      break;
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    case FORM_INPUT_RESET:
      Strcat_charp(tmp, "]");
    }
    Strcat_charp(tmp, "</input_alt>");
    switch (v) {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
      Strcat_char(tmp, ']');
      break;
    case FORM_INPUT_RADIO:
      Strcat_char(tmp, ')');
    }
    Strcat_charp(tmp, "</pre_int>");
  }
  return tmp;
}

Str process_button(struct parsed_tag *tag) {
  Str tmp = NULL;
  char *p, *q, *r, *qq = "";
  int qlen, v;

  if (cur_form_id < 0) {
    char *s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }
  if (tmp == NULL)
    tmp = Strnew();

  p = "submit";
  parsedtag_get_value(tag, ATTR_TYPE, &p);
  q = NULL;
  parsedtag_get_value(tag, ATTR_VALUE, &q);
  r = "";
  parsedtag_get_value(tag, ATTR_NAME, &r);

  v = formtype(p);
  if (v == FORM_UNKNOWN)
    return NULL;

  switch (v) {
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
  case FORM_INPUT_RESET:
    break;
  default:
    p = "submit";
    v = FORM_INPUT_SUBMIT;
    break;
  }

  if (!q) {
    switch (v) {
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      q = "SUBMIT";
      break;
    case FORM_INPUT_RESET:
      q = "RESET";
      break;
    }
  }
  if (q) {
    qq = html_quote(q);
    qlen = strlen(q);
  }

  /*    Strcat_charp(tmp, "<pre_int>"); */
  Strcat(tmp,
         Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=\"%s\" "
                 "name=\"%s\" value=\"%s\">",
                 cur_hseq++, cur_form_id, html_quote(p), html_quote(r), qq));
  return tmp;
}

Str process_n_button(void) {
  Str tmp = Strnew();
  Strcat_charp(tmp, "</input_alt>");
  /*    Strcat_charp(tmp, "</pre_int>"); */
  return tmp;
}

Str process_select(struct parsed_tag *tag) {
  Str tmp = NULL;
  char *p;

  if (cur_form_id < 0) {
    char *s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }

  p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_select = Strnew_charp(p);
  select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

  select_str = Strnew();
  cur_option = NULL;
  cur_status = R_ST_NORMAL;
  n_selectitem = 0;
  return tmp;
}

Str process_n_select(void) {
  if (cur_select == NULL)
    return NULL;
  process_option();
  Strcat_charp(select_str, "<br>");
  cur_select = NULL;
  n_selectitem = 0;
  return select_str;
}

void feed_select(char *str) {
  Str tmp = Strnew();
  int prev_status = cur_status;
  static int prev_spaces = -1;
  char *p;

  if (cur_select == NULL)
    return;
  while (read_token(tmp, &str, &cur_status, 0, 0)) {
    if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
      continue;
    p = tmp->ptr;
    if (tmp->ptr[0] == '<' && Strlastchar(tmp) == '>') {
      struct parsed_tag *tag;
      char *q;
      if (!(tag = parse_tag(&p, FALSE)))
        continue;
      switch (tag->tagid) {
      case HTML_OPTION:
        process_option();
        cur_option = Strnew();
        if (parsedtag_get_value(tag, ATTR_VALUE, &q))
          cur_option_value = Strnew_charp(q);
        else
          cur_option_value = NULL;
        if (parsedtag_get_value(tag, ATTR_LABEL, &q))
          cur_option_label = Strnew_charp(q);
        else
          cur_option_label = NULL;
        cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
        prev_spaces = -1;
        break;
      case HTML_N_OPTION:
        /* do nothing */
        break;
      default:
        /* never happen */
        break;
      }
    } else if (cur_option) {
      while (*p) {
        if (IS_SPACE(*p) && prev_spaces != 0) {
          p++;
          if (prev_spaces > 0)
            prev_spaces++;
        } else {
          if (IS_SPACE(*p))
            prev_spaces = 1;
          else
            prev_spaces = 0;
          if (*p == '&')
            Strcat_charp(cur_option, getescapecmd(&p));
          else
            Strcat_char(cur_option, *(p++));
        }
      }
    }
  }
}

void process_option(void) {
  char begin_char = '[', end_char = ']';
  int len;

  if (cur_select == NULL || cur_option == NULL)
    return;
  while (cur_option->length > 0 && IS_SPACE(Strlastchar(cur_option)))
    Strshrink(cur_option, 1);
  if (cur_option_value == NULL)
    cur_option_value = cur_option;
  if (cur_option_label == NULL)
    cur_option_label = cur_option;
  if (!select_is_multiple) {
    begin_char = '(';
    end_char = ')';
  }
  Strcat(select_str, Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, cur_hseq++, cur_form_id,
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
  if (cur_option_selected)
    Strcat_charp(select_str, " checked>*</input_alt>");
  else
    Strcat_charp(select_str, "> </input_alt>");
  Strcat_char(select_str, end_char);
  Strcat_charp(select_str, html_quote(cur_option_label->ptr));
  Strcat_charp(select_str, "</pre_int>");
  n_selectitem++;
}

Str process_textarea(struct parsed_tag *tag, int width) {
  Str tmp = NULL;
  char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

  if (cur_form_id < 0) {
    char *s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, TRUE));
  }

  p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_textarea = Strnew_charp(p);
  cur_textarea_size = 20;
  if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
    cur_textarea_size = atoi(p);
    if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
      cur_textarea_size = width * cur_textarea_size / 100 - 2;
    if (cur_textarea_size <= 0) {
      cur_textarea_size = 20;
    } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
      cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
    }
  }
  cur_textarea_rows = 1;
  if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
    cur_textarea_rows = atoi(p);
    if (cur_textarea_rows <= 0) {
      cur_textarea_rows = 1;
    } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
      cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
    }
  }
  cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
  if (n_textarea >= max_textarea) {
    max_textarea *= 2;
    textarea_str = New_Reuse(Str, textarea_str, max_textarea);
  }
  textarea_str[n_textarea] = Strnew();
  ignore_nl_textarea = TRUE;

  return tmp;
}

Str process_n_textarea(void) {
  Str tmp;
  int i;

  if (cur_textarea == NULL)
    return NULL;

  tmp = Strnew();
  Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      cur_hseq, cur_form_id, html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
  if (cur_textarea_readonly)
    Strcat_charp(tmp, " readonly");
  Strcat_charp(tmp, "><u>");
  for (i = 0; i < cur_textarea_size; i++)
    Strcat_char(tmp, ' ');
  Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
  cur_hseq++;
  n_textarea++;
  cur_textarea = NULL;

  return tmp;
}

void feed_textarea(char *str) {
  if (cur_textarea == NULL)
    return;
  if (ignore_nl_textarea) {
    if (*str == '\r')
      str++;
    if (*str == '\n')
      str++;
  }
  ignore_nl_textarea = FALSE;
  while (*str) {
    if (*str == '&')
      Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
    else if (*str == '\n') {
      Strcat_charp(textarea_str[n_textarea], "\r\n");
      str++;
    } else if (*str == '\r')
      str++;
    else
      Strcat_char(textarea_str[n_textarea], *(str++));
  }
}

Str process_hr(struct parsed_tag *tag, int width, int indent_width) {
  Str tmp = Strnew_charp("<nobr>");
  int w = 0;
  int x = ALIGN_CENTER;
#define HR_ATTR_WIDTH_MAX 65535

  if (width > indent_width)
    width -= indent_width;
  if (parsedtag_get_value(tag, ATTR_WIDTH, &w)) {
    if (w > HR_ATTR_WIDTH_MAX) {
      w = HR_ATTR_WIDTH_MAX;
    }
    w = REAL_WIDTH(w, width);
  } else {
    w = width;
  }

  parsedtag_get_value(tag, ATTR_ALIGN, &x);
  switch (x) {
  case ALIGN_CENTER:
    Strcat_charp(tmp, "<div_int align=center>");
    break;
  case ALIGN_RIGHT:
    Strcat_charp(tmp, "<div_int align=right>");
    break;
  case ALIGN_LEFT:
    Strcat_charp(tmp, "<div_int align=left>");
    break;
  }
  w /= symbol_width;
  if (w <= 0)
    w = 1;
  push_symbol(tmp, HR_SYMBOL, symbol_width, w);
  Strcat_charp(tmp, "</div_int></nobr>");
  return tmp;
}

static Str process_form_int(struct parsed_tag *tag, int fid) {
  char *p, *q, *r, *s, *tg, *n;

  p = "get";
  parsedtag_get_value(tag, ATTR_METHOD, &p);
  q = "!CURRENT_URL!";
  parsedtag_get_value(tag, ATTR_ACTION, &q);
  q = url_encode(remove_space(q), cur_baseURL, cur_document_charset);
  r = NULL;
  s = NULL;
  parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
  tg = NULL;
  parsedtag_get_value(tag, ATTR_TARGET, &tg);
  n = NULL;
  parsedtag_get_value(tag, ATTR_NAME, &n);

  if (fid < 0) {
    form_max++;
    form_sp++;
    fid = form_max;
  } else { /* <form_int> */
    if (form_max < fid)
      form_max = fid;
    form_sp = fid;
  }
  if (forms_size == 0) {
    forms_size = INITIAL_FORM_SIZE;
    forms = New_N(struct FormList *, forms_size);
    form_stack = NewAtom_N(int, forms_size);
  }
  if (forms_size <= form_max) {
    forms_size += form_max;
    forms = New_Reuse(struct FormList *, forms, forms_size);
    form_stack = New_Reuse(int, form_stack, forms_size);
  }
  form_stack[form_sp] = fid;

  forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
  return NULL;
}

Str process_form(struct parsed_tag *tag) { return process_form_int(tag, -1); }

Str process_n_form(void) {
  if (form_sp >= 0)
    form_sp--;
  return NULL;
}

static void clear_ignore_p_flag(int cmd, struct readbuffer *obuf) {
  static int clear_flag_cmd[] = {HTML_HR, HTML_UNKNOWN};
  int i;

  for (i = 0; clear_flag_cmd[i] != HTML_UNKNOWN; i++) {
    if (cmd == clear_flag_cmd[i]) {
      obuf->flag &= ~RB_IGNORE_P;
      return;
    }
  }
}

static void set_alignment(struct readbuffer *obuf, struct parsed_tag *tag) {
  long flag = -1;
  int align;

  if (parsedtag_get_value(tag, ATTR_ALIGN, &align)) {
    switch (align) {
    case ALIGN_CENTER:
      if (DisableCenter)
        flag = RB_LEFT;
      else
        flag = RB_CENTER;
      break;
    case ALIGN_RIGHT:
      flag = RB_RIGHT;
      break;
    case ALIGN_LEFT:
      flag = RB_LEFT;
    }
  }
  RB_SAVE_FLAG(obuf);
  if (flag != -1) {
    RB_SET_ALIGN(obuf, flag);
  }
}

#define CLOSE_P                                                                \
  if (obuf->flag & RB_P) {                                                     \
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);         \
    RB_RESTORE_FLAG(obuf);                                                     \
    obuf->flag &= ~RB_P;                                                       \
  }

#define HTML5_CLOSE_A                                                          \
  do {                                                                         \
    if (obuf->flag & RB_HTML5) {                                               \
      close_anchor(h_env, obuf);                                               \
    }                                                                          \
  } while (0)

#define CLOSE_A                                                                \
  do {                                                                         \
    CLOSE_P;                                                                   \
    if (!(obuf->flag & RB_HTML5)) {                                            \
      close_anchor(h_env, obuf);                                               \
    }                                                                          \
  } while (0)

#define CLOSE_DT                                                               \
  if (obuf->flag & RB_IN_DT) {                                                 \
    obuf->flag &= ~RB_IN_DT;                                                   \
    HTMLlineproc1("</b>", h_env);                                              \
  }

#define PUSH_ENV(cmd)                                                          \
  if (++h_env->envc_real < h_env->nenv) {                                      \
    ++h_env->envc;                                                             \
    envs[h_env->envc].env = cmd;                                               \
    envs[h_env->envc].count = 0;                                               \
    if (h_env->envc <= MAX_INDENT_LEVEL)                                       \
      envs[h_env->envc].indent = envs[h_env->envc - 1].indent + INDENT_INCR;   \
    else                                                                       \
      envs[h_env->envc].indent = envs[h_env->envc - 1].indent;                 \
  }

#define PUSH_ENV_NOINDENT(cmd)                                                 \
  if (++h_env->envc_real < h_env->nenv) {                                      \
    ++h_env->envc;                                                             \
    envs[h_env->envc].env = cmd;                                               \
    envs[h_env->envc].count = 0;                                               \
    envs[h_env->envc].indent = envs[h_env->envc - 1].indent;                   \
  }

#define POP_ENV                                                                \
  if (h_env->envc_real-- < h_env->nenv)                                        \
    h_env->envc--;

static int ul_type(struct parsed_tag *tag, int default_type) {
  char *p;
  if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
    if (!strcasecmp(p, "disc"))
      return (int)'d';
    else if (!strcasecmp(p, "circle"))
      return (int)'c';
    else if (!strcasecmp(p, "square"))
      return (int)'s';
  }
  return default_type;
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

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env) {
  char *p, *q, *r;
  int i, w, x, y, z, count, width;
  struct readbuffer *obuf = h_env->obuf;
  struct environment *envs = h_env->envs;
  Str tmp;
  int hseq;
  int cmd;

  cmd = tag->tagid;

  if (obuf->flag & RB_PRE) {
    switch (cmd) {
    case HTML_NOBR:
    case HTML_N_NOBR:
    case HTML_PRE_INT:
    case HTML_N_PRE_INT:
      return 1;
    }
  }

  switch (cmd) {
  case HTML_B:
    if (obuf->in_bold < FONTSTAT_MAX)
      obuf->in_bold++;
    if (obuf->in_bold > 1)
      return 1;
    return 0;
  case HTML_N_B:
    if (obuf->in_bold == 1 && close_effect0(obuf, HTML_B))
      obuf->in_bold = 0;
    if (obuf->in_bold > 0) {
      obuf->in_bold--;
      if (obuf->in_bold == 0)
        return 0;
    }
    return 1;
  case HTML_I:
    if (obuf->in_italic < FONTSTAT_MAX)
      obuf->in_italic++;
    if (obuf->in_italic > 1)
      return 1;
    return 0;
  case HTML_N_I:
    if (obuf->in_italic == 1 && close_effect0(obuf, HTML_I))
      obuf->in_italic = 0;
    if (obuf->in_italic > 0) {
      obuf->in_italic--;
      if (obuf->in_italic == 0)
        return 0;
    }
    return 1;
  case HTML_U:
    if (obuf->in_under < FONTSTAT_MAX)
      obuf->in_under++;
    if (obuf->in_under > 1)
      return 1;
    return 0;
  case HTML_N_U:
    if (obuf->in_under == 1 && close_effect0(obuf, HTML_U))
      obuf->in_under = 0;
    if (obuf->in_under > 0) {
      obuf->in_under--;
      if (obuf->in_under == 0)
        return 0;
    }
    return 1;
  case HTML_EM:
    HTMLlineproc1("<i>", h_env);
    return 1;
  case HTML_N_EM:
    HTMLlineproc1("</i>", h_env);
    return 1;
  case HTML_STRONG:
    HTMLlineproc1("<b>", h_env);
    return 1;
  case HTML_N_STRONG:
    HTMLlineproc1("</b>", h_env);
    return 1;
  case HTML_Q:
    HTMLlineproc1("`", h_env);
    return 1;
  case HTML_N_Q:
    HTMLlineproc1("'", h_env);
    return 1;
  case HTML_FIGURE:
  case HTML_N_FIGURE:
  case HTML_P:
  case HTML_N_P:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= RB_IGNORE_P;
    if (cmd == HTML_P) {
      set_alignment(obuf, tag);
      obuf->flag |= RB_P;
    }
    return 1;
  case HTML_FIGCAPTION:
  case HTML_N_FIGCAPTION:
  case HTML_BR:
    flushline(h_env, obuf, envs[h_env->envc].indent, 1, h_env->limit);
    h_env->blank_lines = 0;
    return 1;
  case HTML_H:
    if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P))) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    HTMLlineproc1("<b>", h_env);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_H:
    HTMLlineproc1("</b>", h_env);
    if (!(obuf->flag & RB_PREMODE)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    close_anchor(h_env, obuf);
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_UL:
  case HTML_OL:
  case HTML_BLQ:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!(obuf->flag & RB_PREMODE) && (h_env->envc == 0 || cmd == HTML_BLQ))
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    PUSH_ENV(cmd);
    if (cmd == HTML_UL || cmd == HTML_OL) {
      if (parsedtag_get_value(tag, ATTR_START, &count)) {
        envs[h_env->envc].count = count - 1;
      }
    }
    if (cmd == HTML_OL) {
      envs[h_env->envc].type = '1';
      if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
        envs[h_env->envc].type = (int)*p;
      }
    }
    if (cmd == HTML_UL)
      envs[h_env->envc].type = ul_type(tag, 0);
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 1;
  case HTML_N_UL:
  case HTML_N_OL:
  case HTML_N_DL:
  case HTML_N_BLQ:
  case HTML_N_DD:
    CLOSE_DT;
    CLOSE_A;
    if (h_env->envc > 0) {
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
      POP_ENV;
      if (!(obuf->flag & RB_PREMODE) &&
          (h_env->envc == 0 || cmd == HTML_N_BLQ)) {
        do_blankline(h_env, obuf, envs[h_env->envc].indent, INDENT_INCR,
                     h_env->limit);
        obuf->flag |= RB_IGNORE_P;
      }
    }
    close_anchor(h_env, obuf);
    return 1;
  case HTML_DL:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!(obuf->flag & RB_PREMODE) && envs[h_env->envc].env != HTML_DL &&
          envs[h_env->envc].env != HTML_DL_COMPACT &&
          envs[h_env->envc].env != HTML_DD)
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    PUSH_ENV_NOINDENT(cmd);
    if (parsedtag_exists(tag, ATTR_COMPACT))
      envs[h_env->envc].env = HTML_DL_COMPACT;
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_LI:
    CLOSE_A;
    CLOSE_DT;
    if (h_env->envc > 0) {
      Str num;
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
      envs[h_env->envc].count++;
      if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
        count = atoi(p);
        if (count > 0)
          envs[h_env->envc].count = count;
        else
          envs[h_env->envc].count = 0;
      }
      switch (envs[h_env->envc].env) {
      case HTML_UL:
        envs[h_env->envc].type = ul_type(tag, envs[h_env->envc].type);
        for (i = 0; i < INDENT_INCR - 3; i++)
          push_charp(obuf, 1, NBSP, PC_ASCII);
        tmp = Strnew();
        switch (envs[h_env->envc].type) {
        case 'd':
          push_symbol(tmp, UL_SYMBOL_DISC, symbol_width, 1);
          break;
        case 'c':
          push_symbol(tmp, UL_SYMBOL_CIRCLE, symbol_width, 1);
          break;
        case 's':
          push_symbol(tmp, UL_SYMBOL_SQUARE, symbol_width, 1);
          break;
        default:
          push_symbol(tmp, UL_SYMBOL((h_env->envc_real - 1) % MAX_UL_LEVEL),
                      symbol_width, 1);
          break;
        }
        if (symbol_width == 1)
          push_charp(obuf, 1, NBSP, PC_ASCII);
        push_str(obuf, symbol_width, tmp, PC_ASCII);
        push_charp(obuf, 1, NBSP, PC_ASCII);
        set_space_to_prevchar(obuf->prevchar);
        break;
      case HTML_OL:
        if (parsedtag_get_value(tag, ATTR_TYPE, &p))
          envs[h_env->envc].type = (int)*p;
        switch ((envs[h_env->envc].count > 0) ? envs[h_env->envc].type : '1') {
        case 'i':
          num = romanNumeral(envs[h_env->envc].count);
          break;
        case 'I':
          num = romanNumeral(envs[h_env->envc].count);
          Strupper(num);
          break;
        case 'a':
          num = romanAlphabet(envs[h_env->envc].count);
          break;
        case 'A':
          num = romanAlphabet(envs[h_env->envc].count);
          Strupper(num);
          break;
        default:
          num = Sprintf("%d", envs[h_env->envc].count);
          break;
        }
        if (INDENT_INCR >= 4)
          Strcat_charp(num, ". ");
        else
          Strcat_char(num, '.');
        push_spaces(obuf, 1, INDENT_INCR - num->length);
        push_str(obuf, num->length, num, PC_ASCII);
        if (INDENT_INCR >= 4)
          set_space_to_prevchar(obuf->prevchar);
        break;
      default:
        push_spaces(obuf, 1, INDENT_INCR);
        break;
      }
    } else {
      flushline(h_env, obuf, 0, 0, h_env->limit);
    }
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_DT:
    CLOSE_A;
    if (h_env->envc == 0 ||
        (h_env->envc_real < h_env->nenv && envs[h_env->envc].env != HTML_DL &&
         envs[h_env->envc].env != HTML_DL_COMPACT)) {
      PUSH_ENV_NOINDENT(HTML_DL);
    }
    if (h_env->envc > 0) {
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
    }
    if (!(obuf->flag & RB_IN_DT)) {
      HTMLlineproc1("<b>", h_env);
      obuf->flag |= RB_IN_DT;
    }
    obuf->flag |= RB_IGNORE_P;
    return 1;
  case HTML_N_DT:
    if (!(obuf->flag & RB_IN_DT)) {
      return 1;
    }
    obuf->flag &= ~RB_IN_DT;
    HTMLlineproc1("</b>", h_env);
    if (h_env->envc > 0 && envs[h_env->envc].env == HTML_DL)
      flushline(h_env, obuf, envs[h_env->envc - 1].indent, 0, h_env->limit);
    return 1;
  case HTML_DD:
    CLOSE_A;
    CLOSE_DT;
    if (envs[h_env->envc].env == HTML_DL ||
        envs[h_env->envc].env == HTML_DL_COMPACT) {
      PUSH_ENV(HTML_DD);
    }

    if (h_env->envc > 0 && envs[h_env->envc - 1].env == HTML_DL_COMPACT) {
      if (obuf->pos > envs[h_env->envc].indent)
        flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      else
        push_spaces(obuf, 1, envs[h_env->envc].indent - obuf->pos);
    } else
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    /* obuf->flag |= RB_IGNORE_P; */
    return 1;
  case HTML_TITLE:
    close_anchor(h_env, obuf);
    process_title(tag);
    obuf->flag |= RB_TITLE;
    obuf->end_tag = HTML_N_TITLE;
    return 1;
  case HTML_N_TITLE:
    if (!(obuf->flag & RB_TITLE))
      return 1;
    obuf->flag &= ~RB_TITLE;
    obuf->end_tag = 0;
    tmp = process_n_title(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_TITLE_ALT:
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      h_env->title = html_unquote(p);
    return 0;
  case HTML_FRAMESET:
    PUSH_ENV(cmd);
    push_charp(obuf, 9, "--FRAME--", PC_ASCII);
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 0;
  case HTML_N_FRAMESET:
    if (h_env->envc > 0) {
      POP_ENV;
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    return 0;
  case HTML_NOFRAMES:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag |= (RB_NOFRAMES | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  case HTML_N_NOFRAMES:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag &= ~RB_NOFRAMES;
    return 1;
  case HTML_FRAME:
    q = r = NULL;
    parsedtag_get_value(tag, ATTR_SRC, &q);
    parsedtag_get_value(tag, ATTR_NAME, &r);
    if (q) {
      q = html_quote(q);
      push_tag(obuf, Sprintf("<a hseq=\"%d\" href=\"%s\">", cur_hseq++, q)->ptr,
               HTML_A);
      if (r)
        q = html_quote(r);
      push_charp(obuf, get_strwidth(q), q, PC_ASCII);
      push_tag(obuf, "</a>", HTML_N_A);
    }
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    return 0;
  case HTML_HR:
    close_anchor(h_env, obuf);
    tmp = process_hr(tag, h_env->limit, envs[h_env->envc].indent);
    HTMLlineproc1(tmp->ptr, h_env);
    set_space_to_prevchar(obuf->prevchar);
    return 1;
  case HTML_PRE:
    x = parsedtag_exists(tag, ATTR_FOR_TABLE);
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      if (!x)
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    } else
      fillline(obuf, envs[h_env->envc].indent);
    obuf->flag |= (RB_PRE | RB_IGNORE_P);
    /* istr = str; */
    return 1;
  case HTML_N_PRE:
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    if (!(obuf->flag & RB_IGNORE_P)) {
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
      h_env->blank_lines++;
    }
    obuf->flag &= ~RB_PRE;
    close_anchor(h_env, obuf);
    return 1;
  case HTML_PRE_INT:
    i = obuf->line->length;
    append_tags(obuf);
    if (!(obuf->flag & RB_SPECIAL)) {
      set_breakpoint(obuf, obuf->line->length - i);
    }
    obuf->flag |= RB_PRE_INT;
    return 0;
  case HTML_N_PRE_INT:
    push_tag(obuf, "</pre_int>", HTML_N_PRE_INT);
    obuf->flag &= ~RB_PRE_INT;
    if (!(obuf->flag & RB_SPECIAL) && obuf->pos > obuf->bp.pos) {
      set_prevchar(obuf->prevchar, "", 0);
      obuf->prev_ctype = PC_CTRL;
    }
    return 1;
  case HTML_NOBR:
    obuf->flag |= RB_NOBR;
    obuf->nobr_level++;
    return 0;
  case HTML_N_NOBR:
    if (obuf->nobr_level > 0)
      obuf->nobr_level--;
    if (obuf->nobr_level == 0)
      obuf->flag &= ~RB_NOBR;
    return 0;
  case HTML_PRE_PLAIN:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= (RB_PRE | RB_IGNORE_P);
    return 1;
  case HTML_N_PRE_PLAIN:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
    }
    obuf->flag &= ~RB_PRE;
    return 1;
  case HTML_LISTING:
  case HTML_XMP:
  case HTML_PLAINTEXT:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    }
    obuf->flag |= (RB_PLAIN | RB_IGNORE_P);
    switch (cmd) {
    case HTML_LISTING:
      obuf->end_tag = HTML_N_LISTING;
      break;
    case HTML_XMP:
      obuf->end_tag = HTML_N_XMP;
      break;
    case HTML_PLAINTEXT:
      obuf->end_tag = MAX_HTMLTAG;
      break;
    }
    return 1;
  case HTML_N_LISTING:
  case HTML_N_XMP:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P)) {
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
      obuf->flag |= RB_IGNORE_P;
    }
    obuf->flag &= ~RB_PLAIN;
    obuf->end_tag = 0;
    return 1;
  case HTML_SCRIPT:
    obuf->flag |= RB_SCRIPT;
    obuf->end_tag = HTML_N_SCRIPT;
    return 1;
  case HTML_STYLE:
    obuf->flag |= RB_STYLE;
    obuf->end_tag = HTML_N_STYLE;
    return 1;
  case HTML_N_SCRIPT:
    obuf->flag &= ~RB_SCRIPT;
    obuf->end_tag = 0;
    return 1;
  case HTML_N_STYLE:
    obuf->flag &= ~RB_STYLE;
    obuf->end_tag = 0;
    return 1;
  case HTML_A:
    if (obuf->anchor.url)
      close_anchor(h_env, obuf);

    hseq = 0;

    if (parsedtag_get_value(tag, ATTR_HREF, &p))
      obuf->anchor.url = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
      obuf->anchor.target = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_REFERER, &p))
      obuf->anchor.referer = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
      obuf->anchor.title = Strnew_charp(p)->ptr;
    if (parsedtag_get_value(tag, ATTR_ACCESSKEY, &p))
      obuf->anchor.accesskey = (unsigned char)*p;
    if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq))
      obuf->anchor.hseq = hseq;

    if (hseq == 0 && obuf->anchor.url) {
      obuf->anchor.hseq = cur_hseq;
      tmp = process_anchor(tag, h_env->tagbuf->ptr);
      push_tag(obuf, tmp->ptr, HTML_A);
      if (displayLinkNumber)
        HTMLlineproc1(getLinkNumberStr(-1)->ptr, h_env);
      return 1;
    }
    return 0;
  case HTML_N_A:
    close_anchor(h_env, obuf);
    return 1;
  case HTML_IMG:
    if (parsedtag_exists(tag, ATTR_USEMAP))
      HTML5_CLOSE_A;
    tmp = process_img(tag, h_env->limit);
    HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_IMG_ALT:
    if (parsedtag_get_value(tag, ATTR_SRC, &p))
      obuf->img_alt = Strnew_charp(p);
    return 0;
  case HTML_N_IMG_ALT:
    if (obuf->img_alt) {
      if (!close_effect0(obuf, HTML_IMG_ALT))
        push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
      obuf->img_alt = NULL;
    }
    return 1;
  case HTML_INPUT_ALT:
    i = 0;
    if (parsedtag_get_value(tag, ATTR_TOP_MARGIN, &i)) {
      if ((short)i > obuf->top_margin)
        obuf->top_margin = (short)i;
    }
    i = 0;
    if (parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &i)) {
      if ((short)i > obuf->bottom_margin)
        obuf->bottom_margin = (short)i;
    }
    if (parsedtag_get_value(tag, ATTR_HSEQ, &hseq)) {
      obuf->input_alt.hseq = hseq;
    }
    if (parsedtag_get_value(tag, ATTR_FID, &i)) {
      obuf->input_alt.fid = i;
    }
    if (parsedtag_get_value(tag, ATTR_TYPE, &p)) {
      obuf->input_alt.type = Strnew_charp(p);
    }
    if (parsedtag_get_value(tag, ATTR_VALUE, &p)) {
      obuf->input_alt.value = Strnew_charp(p);
    }
    if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
      obuf->input_alt.name = Strnew_charp(p);
    }
    obuf->input_alt.in = 1;
    return 0;
  case HTML_N_INPUT_ALT:
    if (obuf->input_alt.in) {
      if (!close_effect0(obuf, HTML_INPUT_ALT))
        push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
      obuf->input_alt.hseq = 0;
      obuf->input_alt.fid = -1;
      obuf->input_alt.in = 0;
      obuf->input_alt.type = NULL;
      obuf->input_alt.name = NULL;
      obuf->input_alt.value = NULL;
    }
    return 1;
  case HTML_TABLE:
    close_anchor(h_env, obuf);
    if (obuf->table_level + 1 >= MAX_TABLE)
      break;
    obuf->table_level++;
    w = BORDER_NONE;
    /* x: cellspacing, y: cellpadding */
    x = 2;
    y = 1;
    z = 0;
    width = 0;
    if (parsedtag_exists(tag, ATTR_BORDER)) {
      if (parsedtag_get_value(tag, ATTR_BORDER, &w)) {
        if (w > 2)
          w = BORDER_THICK;
        else if (w < 0) { /* weird */
          w = BORDER_THIN;
        }
      } else
        w = BORDER_THIN;
    }
    if (DisplayBorders && w == BORDER_NONE)
      w = BORDER_THIN;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &i)) {
      if (obuf->table_level == 0)
        width = REAL_WIDTH(i, h_env->limit - envs[h_env->envc].indent);
      else
        width = RELATIVE_WIDTH(i);
    }
    if (parsedtag_exists(tag, ATTR_HBORDER))
      w = BORDER_NOWIN;
#define MAX_CELLSPACING 1000
#define MAX_CELLPADDING 1000
#define MAX_VSPACE 1000
    parsedtag_get_value(tag, ATTR_CELLSPACING, &x);
    parsedtag_get_value(tag, ATTR_CELLPADDING, &y);
    parsedtag_get_value(tag, ATTR_VSPACE, &z);
    if (x < 0)
      x = 0;
    if (y < 0)
      y = 0;
    if (z < 0)
      z = 0;
    if (x > MAX_CELLSPACING)
      x = MAX_CELLSPACING;
    if (y > MAX_CELLPADDING)
      y = MAX_CELLPADDING;
    if (z > MAX_VSPACE)
      z = MAX_VSPACE;
    tables[obuf->table_level] = begin_table(w, x, y, z);
    table_mode[obuf->table_level].pre_mode = 0;
    table_mode[obuf->table_level].indent_level = 0;
    table_mode[obuf->table_level].nobr_level = 0;
    table_mode[obuf->table_level].caption = 0;
    table_mode[obuf->table_level].end_tag = 0; /* HTML_UNKNOWN */
#ifndef TABLE_EXPAND
    tables[obuf->table_level]->total_width = width;
#else
    tables[obuf->table_level]->real_width = width;
    tables[obuf->table_level]->total_width = 0;
#endif
    return 1;
  case HTML_N_TABLE:
    /* should be processed in HTMLlineproc() */
    return 1;
  case HTML_CENTER:
    CLOSE_A;
    if (!(obuf->flag & (RB_PREMODE | RB_IGNORE_P)))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_SAVE_FLAG(obuf);
    if (DisableCenter)
      RB_SET_ALIGN(obuf, RB_LEFT);
    else
      RB_SET_ALIGN(obuf, RB_CENTER);
    return 1;
  case HTML_N_CENTER:
    CLOSE_A;
    if (!(obuf->flag & RB_PREMODE))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_DIV:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_DIV:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_DIV_INT:
    CLOSE_P;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    set_alignment(obuf, tag);
    return 1;
  case HTML_N_DIV_INT:
    CLOSE_P;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    RB_RESTORE_FLAG(obuf);
    return 1;
  case HTML_FORM:
    CLOSE_A;
    if (!(obuf->flag & RB_IGNORE_P))
      flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    tmp = process_form(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_N_FORM:
    CLOSE_A;
    flushline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
    obuf->flag |= RB_IGNORE_P;
    process_n_form();
    return 1;
  case HTML_INPUT:
    close_anchor(h_env, obuf);
    tmp = process_input(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_BUTTON:
    HTML5_CLOSE_A;
    tmp = process_button(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_N_BUTTON:
    tmp = process_n_button();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_SELECT:
    close_anchor(h_env, obuf);
    tmp = process_select(tag);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INSELECT;
    obuf->end_tag = HTML_N_SELECT;
    return 1;
  case HTML_N_SELECT:
    obuf->flag &= ~RB_INSELECT;
    obuf->end_tag = 0;
    tmp = process_n_select();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_OPTION:
    /* nothing */
    return 1;
  case HTML_TEXTAREA:
    close_anchor(h_env, obuf);
    tmp = process_textarea(tag, h_env->limit);
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    obuf->flag |= RB_INTXTA;
    obuf->end_tag = HTML_N_TEXTAREA;
    return 1;
  case HTML_N_TEXTAREA:
    obuf->flag &= ~RB_INTXTA;
    obuf->end_tag = 0;
    tmp = process_n_textarea();
    if (tmp)
      HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_ISINDEX:
    p = "";
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_PROMPT, &p);
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    tmp = Strnew_m_charp("<form method=get action=\"", html_quote(q), "\">",
                         html_quote(p),
                         "<input type=text name=\"\" accept></form>", NULL);
    HTMLlineproc1(tmp->ptr, h_env);
    return 1;
  case HTML_DOCTYPE:
    if (!parsedtag_exists(tag, ATTR_PUBLIC)) {
      obuf->flag |= RB_HTML5;
    }
    return 1;
  case HTML_META:
    p = q = r = NULL;
    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
    parsedtag_get_value(tag, ATTR_CONTENT, &q);
    if (p && q && !strcasecmp(p, "refresh")) {
      int refresh_interval;
      tmp = NULL;
      refresh_interval = getMetaRefreshParam(q, &tmp);
      if (tmp) {
        q = html_quote(tmp->ptr);
        tmp = Sprintf("Refresh (%d sec) <a href=\"%s\">%s</a>",
                      refresh_interval, q, q);
      } else if (refresh_interval > 0)
        tmp = Sprintf("Refresh (%d sec)", refresh_interval);
      if (tmp) {
        HTMLlineproc1(tmp->ptr, h_env);
        do_blankline(h_env, obuf, envs[h_env->envc].indent, 0, h_env->limit);
        if (!is_redisplay && !((obuf->flag & RB_NOFRAMES) && RenderFrame)) {
          tag->need_reconstruct = TRUE;
          return 0;
        }
      }
    }
    return 1;
  case HTML_BASE:
#if defined(USE_M17N) || defined(USE_IMAGE)
    p = NULL;
    if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
      cur_baseURL = New(struct Url);
      parseURL(p, cur_baseURL, NULL);
    }
#endif
  case HTML_MAP:
  case HTML_N_MAP:
  case HTML_AREA:
    return 0;
  case HTML_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag |= RB_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[DEL:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike < FONTSTAT_MAX)
        obuf->in_strike++;
      if (obuf->in_strike == 1) {
        push_tag(obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  case HTML_N_DEL:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag &= ~RB_DEL;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:DEL]</U>", h_env);
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike == 0)
        return 1;
      if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
        obuf->in_strike = 0;
      if (obuf->in_strike > 0) {
        obuf->in_strike--;
        if (obuf->in_strike == 0) {
          push_tag(obuf, "</s>", HTML_N_S);
        }
      }
      break;
    }
    return 1;
  case HTML_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag |= RB_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[S:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike < FONTSTAT_MAX)
        obuf->in_strike++;
      if (obuf->in_strike == 1) {
        push_tag(obuf, "<s>", HTML_S);
      }
      break;
    }
    return 1;
  case HTML_N_S:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      obuf->flag &= ~RB_S;
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:S]</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_strike == 0)
        return 1;
      if (obuf->in_strike == 1 && close_effect0(obuf, HTML_S))
        obuf->in_strike = 0;
      if (obuf->in_strike > 0) {
        obuf->in_strike--;
        if (obuf->in_strike == 0) {
          push_tag(obuf, "</s>", HTML_N_S);
        }
      }
    }
    return 1;
  case HTML_INS:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>[INS:</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_ins < FONTSTAT_MAX)
        obuf->in_ins++;
      if (obuf->in_ins == 1) {
        push_tag(obuf, "<ins>", HTML_INS);
      }
      break;
    }
    return 1;
  case HTML_N_INS:
    switch (displayInsDel) {
    case DISPLAY_INS_DEL_SIMPLE:
      break;
    case DISPLAY_INS_DEL_NORMAL:
      HTMLlineproc1("<U>:INS]</U>", h_env);
      break;
    case DISPLAY_INS_DEL_FONTIFY:
      if (obuf->in_ins == 0)
        return 1;
      if (obuf->in_ins == 1 && close_effect0(obuf, HTML_INS))
        obuf->in_ins = 0;
      if (obuf->in_ins > 0) {
        obuf->in_ins--;
        if (obuf->in_ins == 0) {
          push_tag(obuf, "</ins>", HTML_N_INS);
        }
      }
      break;
    }
    return 1;
  case HTML_SUP:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("^", h_env);
    return 1;
  case HTML_N_SUP:
    return 1;
  case HTML_SUB:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("[", h_env);
    return 1;
  case HTML_N_SUB:
    if (!(obuf->flag & (RB_DEL | RB_S)))
      HTMLlineproc1("]", h_env);
    return 1;
  case HTML_FONT:
  case HTML_N_FONT:
  case HTML_NOP:
    return 1;
  case HTML_BGSOUND:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
        Str s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">bgsound(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_EMBED:
    HTML5_CLOSE_A;
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
        Str s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">embed(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_APPLET:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_ARCHIVE, &p)) {
        Str s;
        q = html_quote(p);
        s = Sprintf("<A HREF=\"%s\">applet archive(%s)</A>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
    return 1;
  case HTML_BODY:
    if (view_unseenobject) {
      if (parsedtag_get_value(tag, ATTR_BACKGROUND, &p)) {
        Str s;
        q = html_quote(p);
        s = Sprintf("<IMG SRC=\"%s\" ALT=\"bg image(%s)\"><BR>", q, q);
        HTMLlineproc1(s->ptr, h_env);
      }
    }
  case HTML_N_HEAD:
    if (obuf->flag & RB_TITLE)
      HTMLlineproc1("</title>", h_env);
  case HTML_HEAD:
  case HTML_N_BODY:
    return 1;
  default:
    /* obuf->prevchar = '\0'; */
    return 0;
  }
  /* not reached */
  return 0;
}

#define PPUSH(p, c)                                                            \
  {                                                                            \
    outp[pos] = (p);                                                           \
    outc[pos] = (c);                                                           \
    pos++;                                                                     \
  }
#define PSIZE                                                                  \
  if (out_size <= pos + 1) {                                                   \
    out_size = pos * 3 / 2;                                                    \
    outc = New_Reuse(char, outc, out_size);                                    \
    outp = New_Reuse(Lineprop, outp, out_size);                                \
  }

static TextLineListItem *_tl_lp2;

static Str textlist_feed() {
  TextLine *p;
  if (_tl_lp2 != NULL) {
    p = _tl_lp2->ptr;
    _tl_lp2 = _tl_lp2->next;
    return p->line;
  }
  return NULL;
}

static int ex_efct(int ex) {
  int effect = 0;

  if (!ex)
    return 0;

  if (ex & PE_EX_ITALIC)
    effect |= PE_EX_ITALIC_E;

  if (ex & PE_EX_INSERT)
    effect |= PE_EX_INSERT_E;

  if (ex & PE_EX_STRIKE)
    effect |= PE_EX_STRIKE_E;

  return effect;
}

static void HTMLlineproc2body(struct Buffer *buf, Str (*feed)(), int llimit) {
  static char *outc = NULL;
  static Lineprop *outp = NULL;
  static int out_size = 0;
  Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
  char *p, *q, *r, *s, *t, *str;
  Lineprop mode, effect, ex_effect;
  int pos;
  int nlines;
#ifdef DEBUG
  FILE *debug = NULL;
#endif
  struct frameset *frameset_s[FRAMESTACK_SIZE];
  int frameset_sp = -1;
  union frameset_element *idFrame = NULL;
  char *id = NULL;
  int hseq, form_id;
  Str line;
  char *endp;
  char symbol = '\0';
  int internal = 0;
  Anchor **a_textarea = NULL;
#if defined(USE_M17N) || defined(USE_IMAGE)
  struct Url *base = baseURL(buf);
#endif

  if (out_size == 0) {
    out_size = LINELEN;
    outc = NewAtom_N(char, out_size);
    outp = NewAtom_N(Lineprop, out_size);
  }

  n_textarea = -1;
  if (!max_textarea) { /* halfload */
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
    a_textarea = New_N(Anchor *, max_textarea);
  }

#ifdef DEBUG
  if (w3m_debug)
    debug = fopen("zzzerr", "a");
#endif

  effect = 0;
  ex_effect = 0;
  nlines = 0;
  while ((line = feed()) != NULL) {
#ifdef DEBUG
    if (w3m_debug) {
      Strfputs(line, debug);
      fputc('\n', debug);
    }
#endif
    if (n_textarea >= 0 && *(line->ptr) != '<') { /* halfload */
      Strcat(textarea_str[n_textarea], line);
      continue;
    }
  proc_again:
    if (++nlines == llimit)
      break;
    pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
    Strremovetrailingspaces(line);
#endif
    str = line->ptr;
    endp = str + line->length;
    while (str < endp) {
      PSIZE;
      mode = get_mctype(str);
      if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<') {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
        str += symbol_width;
      } else if (mode == PC_CTRL || IS_INTSPACE(*str)) {
        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
        str++;
      } else if (*str != '<' && *str != '&') {
        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
      } else if (*str == '&') {
        /*
         * & escape processing
         */
        p = getescapecmd(&str);
        while (*p) {
          PSIZE;
          mode = get_mctype((unsigned char *)p);
          if (mode == PC_CTRL || IS_INTSPACE(*str)) {
            PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
            p++;
          } else {
            PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
          }
        }
      } else {
        /* tag processing */
        struct parsed_tag *tag;
        if (!(tag = parse_tag(&str, TRUE)))
          continue;
        switch (tag->tagid) {
        case HTML_B:
          effect |= PE_BOLD;
          break;
        case HTML_N_B:
          effect &= ~PE_BOLD;
          break;
        case HTML_I:
          ex_effect |= PE_EX_ITALIC;
          break;
        case HTML_N_I:
          ex_effect &= ~PE_EX_ITALIC;
          break;
        case HTML_INS:
          ex_effect |= PE_EX_INSERT;
          break;
        case HTML_N_INS:
          ex_effect &= ~PE_EX_INSERT;
          break;
        case HTML_U:
          effect |= PE_UNDER;
          break;
        case HTML_N_U:
          effect &= ~PE_UNDER;
          break;
        case HTML_S:
          ex_effect |= PE_EX_STRIKE;
          break;
        case HTML_N_S:
          ex_effect &= ~PE_EX_STRIKE;
          break;
        case HTML_A:
          if (renderFrameSet && parsedtag_get_value(tag, ATTR_FRAMENAME, &p)) {
            p = url_quote_conv(p, buf->document_charset);
            if (!idFrame || strcmp(idFrame->body->name, p)) {
              idFrame = search_frame(renderFrameSet, p);
              if (idFrame && idFrame->body->attr != F_BODY)
                idFrame = NULL;
            }
          }
          p = r = s = NULL;
          q = buf->baseTarget;
          t = "";
          hseq = 0;
          id = NULL;
          if (parsedtag_get_value(tag, ATTR_NAME, &id)) {
            id = url_quote_conv(id, name_charset);
            registerName(buf, id, currentLn(buf), pos);
          }
          if (parsedtag_get_value(tag, ATTR_HREF, &p))
            p = url_encode(remove_space(p), base, buf->document_charset);
          if (parsedtag_get_value(tag, ATTR_TARGET, &q))
            q = url_quote_conv(q, buf->document_charset);
          if (parsedtag_get_value(tag, ATTR_REFERER, &r))
            r = url_encode(r, base, buf->document_charset);
          parsedtag_get_value(tag, ATTR_TITLE, &s);
          parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
          parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
          if (hseq > 0)
            buf->hmarklist =
                putHmarker(buf->hmarklist, currentLn(buf), pos, hseq - 1);
          else if (hseq < 0) {
            int h = -hseq - 1;
            if (buf->hmarklist && h < buf->hmarklist->nmark &&
                buf->hmarklist->marks[h].invalid) {
              buf->hmarklist->marks[h].pos = pos;
              buf->hmarklist->marks[h].line = currentLn(buf);
              buf->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }
          if (id && idFrame)
            idFrame->body->nameList =
                putAnchor(idFrame->body->nameList, id, NULL, (Anchor **)NULL,
                          NULL, NULL, '\0', currentLn(buf), pos);
          if (p) {
            effect |= PE_ANCHOR;
            a_href = registerHref(buf, p, q, r, s, *t, currentLn(buf), pos);
            a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
            a_href->slave = (hseq > 0) ? FALSE : TRUE;
          }
          break;
        case HTML_N_A:
          effect &= ~PE_ANCHOR;
          if (a_href) {
            a_href->end.line = currentLn(buf);
            a_href->end.pos = pos;
            if (a_href->start.line == a_href->end.line &&
                a_href->start.pos == a_href->end.pos) {
              if (buf->hmarklist && a_href->hseq >= 0 &&
                  a_href->hseq < buf->hmarklist->nmark)
                buf->hmarklist->marks[a_href->hseq].invalid = 1;
              a_href->hseq = -1;
            }
            a_href = NULL;
          }
          break;

        case HTML_LINK:
          addLink(buf, tag);
          break;

        case HTML_IMG_ALT:
          if (parsedtag_get_value(tag, ATTR_SRC, &p)) {
            s = NULL;
            parsedtag_get_value(tag, ATTR_TITLE, &s);
            p = url_quote_conv(remove_space(p), buf->document_charset);
            a_img = registerImg(buf, p, s, currentLn(buf), pos);
          }
          effect |= PE_IMAGE;
          break;
        case HTML_N_IMG_ALT:
          effect &= ~PE_IMAGE;
          if (a_img) {
            a_img->end.line = currentLn(buf);
            a_img->end.pos = pos;
          }
          a_img = NULL;
          break;
        case HTML_INPUT_ALT: {
          struct FormList *form;
          int top = 0, bottom = 0;
          int textareanumber = -1;
          hseq = 0;
          form_id = -1;

          parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
          parsedtag_get_value(tag, ATTR_FID, &form_id);
          parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
          parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
          if (form_id < 0 || form_id > form_max || forms == NULL ||
              forms[form_id] == NULL)
            break; /* outside of <form>..</form> */
          form = forms[form_id];
          if (hseq > 0) {
            int hpos = pos;
            if (*str == '[')
              hpos++;
            buf->hmarklist =
                putHmarker(buf->hmarklist, currentLn(buf), hpos, hseq - 1);
          } else if (hseq < 0) {
            int h = -hseq - 1;
            int hpos = pos;
            if (*str == '[')
              hpos++;
            if (buf->hmarklist && h < buf->hmarklist->nmark &&
                buf->hmarklist->marks[h].invalid) {
              buf->hmarklist->marks[h].pos = hpos;
              buf->hmarklist->marks[h].line = currentLn(buf);
              buf->hmarklist->marks[h].invalid = 0;
              hseq = -hseq;
            }
          }

          if (!form->target)
            form->target = buf->baseTarget;
          if (a_textarea &&
              parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &textareanumber)) {
            if (textareanumber >= max_textarea) {
              max_textarea = 2 * textareanumber;
              textarea_str = New_Reuse(Str, textarea_str, max_textarea);
              a_textarea = New_Reuse(Anchor *, a_textarea, max_textarea);
            }
          }
          a_form = registerForm(buf, form, tag, currentLn(buf), pos);
          if (a_textarea && textareanumber >= 0)
            a_textarea[textareanumber] = a_form;
          if (a_form) {
            a_form->hseq = hseq - 1;
            a_form->y = currentLn(buf) - top;
            a_form->rows = 1 + top + bottom;
            if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
              effect |= PE_FORM;
            break;
          }
        }
        case HTML_N_INPUT_ALT:
          effect &= ~PE_FORM;
          if (a_form) {
            a_form->end.line = currentLn(buf);
            a_form->end.pos = pos;
            if (a_form->start.line == a_form->end.line &&
                a_form->start.pos == a_form->end.pos)
              a_form->hseq = -1;
          }
          a_form = NULL;
          break;
        case HTML_MAP:
          if (parsedtag_get_value(tag, ATTR_NAME, &p)) {
            struct MapList *m = New(struct MapList);
            m->name = Strnew_charp(p);
            m->area = newGeneralList();
            m->next = buf->maplist;
            buf->maplist = m;
          }
          break;
        case HTML_N_MAP:
          /* nothing to do */
          break;
        case HTML_AREA:
          if (buf->maplist == NULL) /* outside of <map>..</map> */
            break;
          if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            struct MapArea *a;
            p = url_encode(remove_space(p), base, buf->document_charset);
            t = NULL;
            parsedtag_get_value(tag, ATTR_TARGET, &t);
            q = "";
            parsedtag_get_value(tag, ATTR_ALT, &q);
            r = NULL;
            s = NULL;
            a = newMapArea(p, t, q, r, s);
            pushValue(buf->maplist->area, (void *)a);
          }
          break;
        case HTML_FRAMESET:
          frameset_sp++;
          if (frameset_sp >= FRAMESTACK_SIZE)
            break;
          frameset_s[frameset_sp] = newFrameSet(tag);
          if (frameset_s[frameset_sp] == NULL)
            break;
          if (frameset_sp == 0) {
            if (buf->frameset == NULL) {
              buf->frameset = frameset_s[frameset_sp];
            } else
              pushFrameTree(&(buf->frameQ), frameset_s[frameset_sp], NULL);
          } else
            addFrameSetElement(
                frameset_s[frameset_sp - 1],
                *(union frameset_element *)&frameset_s[frameset_sp]);
          break;
        case HTML_N_FRAMESET:
          if (frameset_sp >= 0)
            frameset_sp--;
          break;
        case HTML_FRAME:
          if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE) {
            union frameset_element element;

            element.body = newFrame(tag, buf);
            addFrameSetElement(frameset_s[frameset_sp], element);
          }
          break;
        case HTML_BASE:
          if (parsedtag_get_value(tag, ATTR_HREF, &p)) {
            p = url_encode(remove_space(p), NULL, buf->document_charset);
            if (!buf->baseURL)
              buf->baseURL = New(struct Url);
            parseURL2(p, buf->baseURL, &buf->currentURL);
#if defined(USE_M17N) || defined(USE_IMAGE)
            base = buf->baseURL;
#endif
          }
          if (parsedtag_get_value(tag, ATTR_TARGET, &p))
            buf->baseTarget = url_quote_conv(p, buf->document_charset);
          break;
        case HTML_META:
          p = q = NULL;
          parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
          parsedtag_get_value(tag, ATTR_CONTENT, &q);
          if (p && q && !strcasecmp(p, "refresh") && MetaRefresh) {
            Str tmp = NULL;
            int refresh_interval = getMetaRefreshParam(q, &tmp);
            if (tmp) {
              p = url_encode(remove_space(tmp->ptr), base,
                             buf->document_charset);
            }
          }
          break;
        case HTML_INTERNAL:
          internal = HTML_INTERNAL;
          break;
        case HTML_N_INTERNAL:
          internal = HTML_N_INTERNAL;
          break;
        case HTML_FORM_INT:
          if (parsedtag_get_value(tag, ATTR_FID, &form_id))
            process_form_int(tag, form_id);
          break;
        case HTML_TEXTAREA_INT:
          if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER, &n_textarea) &&
              n_textarea >= 0 && n_textarea < max_textarea) {
            textarea_str[n_textarea] = Strnew();
          } else
            n_textarea = -1;
          break;
        case HTML_N_TEXTAREA_INT:
          if (a_textarea && n_textarea >= 0) {
            struct FormItemList *item =
                (struct FormItemList *)a_textarea[n_textarea]->url;
            item->init_value = item->value = textarea_str[n_textarea];
          }
          break;
        case HTML_TITLE_ALT:
          if (parsedtag_get_value(tag, ATTR_TITLE, &p))
            buf->buffername = html_unquote(p);
          break;
        case HTML_SYMBOL:
          effect |= PC_SYMBOL;
          if (parsedtag_get_value(tag, ATTR_TYPE, &p))
            symbol = (char)atoi(p);
          break;
        case HTML_N_SYMBOL:
          effect &= ~PC_SYMBOL;
          break;
        }
      }
    }
    /* end of processing for one line */
    if (!internal)
      addnewline(buf, outc, outp, NULL, pos, -1, nlines);
    if (internal == HTML_N_INTERNAL)
      internal = 0;
    if (str != endp) {
      line = Strsubstr(line, str - line->ptr, endp - str);
      goto proc_again;
    }
  }
#ifdef DEBUG
  if (w3m_debug)
    fclose(debug);
#endif
  for (form_id = 1; form_id <= form_max; form_id++)
    if (forms[form_id])
      forms[form_id]->next = forms[form_id - 1];
  buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
  if (n_textarea)
    addMultirowsForm(buf, buf->formitem);
}

static void addLink(struct Buffer *buf, struct parsed_tag *tag) {
  char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL, *rev = NULL;

  parsedtag_get_value(tag, ATTR_HREF, &href);
  if (href)
    href = url_encode(remove_space(href), baseURL(buf), buf->document_charset);
  parsedtag_get_value(tag, ATTR_TITLE, &title);
  parsedtag_get_value(tag, ATTR_TYPE, &ctype);
  parsedtag_get_value(tag, ATTR_REL, &rel);

  char type = LINK_TYPE_NONE;
  if (rel != NULL) {
    /* forward link type */
    type = LINK_TYPE_REL;
    if (title == NULL)
      title = rel;
  }
  parsedtag_get_value(tag, ATTR_REV, &rev);
  if (rev != NULL) {
    /* reverse link type */
    type = LINK_TYPE_REV;
    if (title == NULL)
      title = rev;
  }

  struct LinkList* l = New(struct LinkList);
  l->url = href;
  l->title = title;
  l->ctype = ctype;
  l->type = type;
  l->next = NULL;
  if (buf->linklist) {
    struct LinkList *i;
    for (i = buf->linklist; i->next; i = i->next)
      ;
    i->next = l;
  } else
    buf->linklist = l;
}

void HTMLlineproc2(struct Buffer *buf, TextLineList *tl) {
  _tl_lp2 = tl->first;
  HTMLlineproc2body(buf, textlist_feed, -1);
}

static InputStream _file_lp2;

static Str file_feed() {
  Str s;
  s = StrISgets(_file_lp2);
  if (s->length == 0) {
    ISclose(_file_lp2);
    return NULL;
  }
  return s;
}

void HTMLlineproc3(struct Buffer *buf, InputStream stream) {
  _file_lp2 = stream;
  HTMLlineproc2body(buf, file_feed, -1);
}

static void proc_escape(struct readbuffer *obuf, char **str_return) {
  char *str = *str_return, *estr;
  int ech = getescapechar(str_return);
  int width, n_add = *str_return - str;
  Lineprop mode = PC_ASCII;

  if (ech < 0) {
    *str_return = str;
    proc_mchar(obuf, obuf->flag & RB_SPECIAL, 1, str_return, PC_ASCII);
    return;
  }
  mode = IS_CNTRL(ech) ? PC_CTRL : PC_ASCII;

  estr = conv_entity(ech);
  check_breakpoint(obuf, obuf->flag & RB_SPECIAL, estr);
  width = get_strwidth(estr);
  if (width == 1 && ech == (unsigned char)*estr && ech != '&' && ech != '<' &&
      ech != '>') {
    if (IS_CNTRL(ech))
      mode = PC_CTRL;
    push_charp(obuf, width, estr, mode);
  } else
    push_nchars(obuf, width, str, n_add, mode);
  set_prevchar(obuf->prevchar, estr, strlen(estr));
  obuf->prev_ctype = mode;
}

static int need_flushline(struct html_feed_environ *h_env,
                          struct readbuffer *obuf, Lineprop mode) {
  char ch;

  if (obuf->flag & RB_PRE_INT) {
    if (obuf->pos > h_env->limit)
      return 1;
    else
      return 0;
  }

  ch = Strlastchar(obuf->line);
  /* if (ch == ' ' && obuf->tag_sp > 0) */
  if (ch == ' ')
    return 0;

  if (obuf->pos > h_env->limit)
    return 1;

  return 0;
}

static int table_width(struct html_feed_environ *h_env, int table_level) {
  int width;
  if (table_level < 0)
    return 0;
  width = tables[table_level]->total_width;
  if (table_level > 0 || width > 0)
    return width;
  return h_env->limit - h_env->envs[h_env->envc].indent;
}

/* HTML processing first pass */
void HTMLlineproc0(char *line, struct html_feed_environ *h_env, int internal) {
  Lineprop mode;
  int cmd;
  struct readbuffer *obuf = h_env->obuf;
  int indent, delta;
  struct parsed_tag *tag;
  Str tokbuf;
  struct table *tbl = NULL;
  struct table_mode *tbl_mode = NULL;
  int tbl_width = 0;

#ifdef DEBUG
  if (w3m_debug) {
    FILE *f = fopen("zzzproc1", "a");
    fprintf(f, "%c%c%c%c", (obuf->flag & RB_PREMODE) ? 'P' : ' ',
            (obuf->table_level >= 0) ? 'T' : ' ',
            (obuf->flag & RB_INTXTA) ? 'X' : ' ',
            (obuf->flag & (RB_SCRIPT | RB_STYLE)) ? 'S' : ' ');
    fprintf(f, "HTMLlineproc1(\"%s\",%d,%lx)\n", line, h_env->limit,
            (unsigned long)h_env);
    fclose(f);
  }
#endif

  tokbuf = Strnew();

table_start:
  if (obuf->table_level >= 0) {
    int level = min(obuf->table_level, MAX_TABLE - 1);
    tbl = tables[level];
    tbl_mode = &table_mode[level];
    tbl_width = table_width(h_env, level);
  }

  while (*line != '\0') {
    char *str, *p;
    int is_tag = FALSE;
    int pre_mode =
        (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode : obuf->flag;
    int end_tag = (obuf->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag
                                                       : obuf->end_tag;

    if (*line == '<' || obuf->status != R_ST_NORMAL) {
      /*
       * Tag processing
       */
      if (obuf->status == R_ST_EOL)
        obuf->status = R_ST_NORMAL;
      else {
        read_token(h_env->tagbuf, &line, &obuf->status, pre_mode & RB_PREMODE,
                   obuf->status != R_ST_NORMAL);
        if (obuf->status != R_ST_NORMAL)
          return;
      }
      if (h_env->tagbuf->length == 0)
        continue;
      str = Strdup(h_env->tagbuf)->ptr;
      if (*str == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = TRUE;
        else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                               RB_STYLE | RB_TITLE))) {
          line = Strnew_m_charp(str + 1, line, NULL)->ptr;
          str = "&lt;";
        }
      }
    } else {
      read_token(tokbuf, &line, &obuf->status, pre_mode & RB_PREMODE, 0);
      if (obuf->status != R_ST_NORMAL) /* R_ST_AMP ? */
        obuf->status = R_ST_NORMAL;
      str = tokbuf->ptr;
    }

    if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE |
                    RB_TITLE)) {
      if (is_tag) {
        p = str;
        if ((tag = parse_tag(&p, internal))) {
          if (tag->tagid == end_tag ||
              (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) ||
              (pre_mode & RB_TITLE &&
               (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY)))
            goto proc_normal;
        }
      }
      /* title */
      if (pre_mode & RB_TITLE) {
        feed_title(str);
        continue;
      }
      /* select */
      if (pre_mode & RB_INSELECT) {
        if (obuf->table_level >= 0)
          goto proc_normal;
        feed_select(str);
        continue;
      }
      if (is_tag) {
        if (strncmp(str, "<!--", 4) && (p = strchr(str + 1, '<'))) {
          str = Strnew_charp_n(str, p - str)->ptr;
          line = Strnew_m_charp(p, line, NULL)->ptr;
        }
        is_tag = FALSE;
      }
      if (obuf->table_level >= 0)
        goto proc_normal;
      /* textarea */
      if (pre_mode & RB_INTXTA) {
        feed_textarea(str);
        continue;
      }
      /* script */
      if (pre_mode & RB_SCRIPT)
        continue;
      /* style */
      if (pre_mode & RB_STYLE)
        continue;
    }

  proc_normal:
    if (obuf->table_level >= 0 && tbl && tbl_mode) {
      /*
       * within table: in <table>..</table>, all input tokens
       * are fed to the table renderer, and then the renderer
       * makes HTML output.
       */
      switch (feed_table(tbl, str, tbl_mode, tbl_width, internal)) {
      case 0:
        /* </table> tag */
        obuf->table_level--;
        if (obuf->table_level >= MAX_TABLE - 1)
          continue;
        end_table(tbl);
        if (obuf->table_level >= 0) {
          struct table *tbl0 = tables[obuf->table_level];
          str = Sprintf("<table_alt tid=%d>", tbl0->ntable)->ptr;
          if (tbl0->row < 0)
            continue;
          pushTable(tbl0, tbl);
          tbl = tbl0;
          tbl_mode = &table_mode[obuf->table_level];
          tbl_width = table_width(h_env, obuf->table_level);
          feed_table(tbl, str, tbl_mode, tbl_width, TRUE);
          continue;
          /* continue to the next */
        }
        if (obuf->flag & RB_DEL)
          continue;
        /* all tables have been read */
        if (tbl->vspace > 0 && !(obuf->flag & RB_IGNORE_P)) {
          int indent = h_env->envs[h_env->envc].indent;
          flushline(h_env, obuf, indent, 0, h_env->limit);
          do_blankline(h_env, obuf, indent, 0, h_env->limit);
        }
        save_fonteffect(h_env, obuf);
        initRenderTable();
        renderTable(tbl, tbl_width, h_env);
        restore_fonteffect(h_env, obuf);
        obuf->flag &= ~RB_IGNORE_P;
        if (tbl->vspace > 0) {
          int indent = h_env->envs[h_env->envc].indent;
          do_blankline(h_env, obuf, indent, 0, h_env->limit);
          obuf->flag |= RB_IGNORE_P;
        }
        set_space_to_prevchar(obuf->prevchar);
        continue;
      case 1:
        /* <table> tag */
        break;
      default:
        continue;
      }
    }

    if (is_tag) {
      /*** Beginning of a new tag ***/
      if ((tag = parse_tag(&str, internal)))
        cmd = tag->tagid;
      else
        continue;
      /* process tags */
      if (HTMLtagproc1(tag, h_env) == 0) {
        /* preserve the tag for second-stage processing */
        if (parsedtag_need_reconstruct(tag))
          h_env->tagbuf = parsedtag2str(tag);
        push_tag(obuf, h_env->tagbuf->ptr, cmd);
      }
      obuf->bp.init_flag = 1;
      clear_ignore_p_flag(cmd, obuf);
      if (cmd == HTML_TABLE)
        goto table_start;
      else
        continue;
    }

    if (obuf->flag & (RB_DEL | RB_S))
      continue;
    while (*str) {
      mode = get_mctype(str);
      delta = get_mcwidth(str);
      if (obuf->flag & (RB_SPECIAL & ~RB_NOBR)) {
        char ch = *str;
        if (!(obuf->flag & RB_PLAIN) && (*str == '&')) {
          char *p = str;
          int ech = getescapechar(&p);
          if (ech == '\n' || ech == '\r') {
            ch = '\n';
            str = p - 1;
          } else if (ech == '\t') {
            ch = '\t';
            str = p - 1;
          }
        }
        if (ch != '\n')
          obuf->flag &= ~RB_IGNORE_P;
        if (ch == '\n') {
          str++;
          if (obuf->flag & RB_IGNORE_P) {
            obuf->flag &= ~RB_IGNORE_P;
            continue;
          }
          if (obuf->flag & RB_PRE_INT)
            PUSH(' ');
          else
            flushline(h_env, obuf, h_env->envs[h_env->envc].indent, 1,
                      h_env->limit);
        } else if (ch == '\t') {
          do {
            PUSH(' ');
          } while ((h_env->envs[h_env->envc].indent + obuf->pos) % Tabstop !=
                   0);
          str++;
        } else if (obuf->flag & RB_PLAIN) {
          char *p = html_quote_char(*str);
          if (p) {
            push_charp(obuf, 1, p, PC_ASCII);
            str++;
          } else {
            proc_mchar(obuf, 1, delta, &str, mode);
          }
        } else {
          if (*str == '&')
            proc_escape(obuf, &str);
          else
            proc_mchar(obuf, 1, delta, &str, mode);
        }
        if (obuf->flag & (RB_SPECIAL & ~RB_PRE_INT))
          continue;
      } else {
        if (!IS_SPACE(*str))
          obuf->flag &= ~RB_IGNORE_P;
        if ((mode == PC_ASCII || mode == PC_CTRL) && IS_SPACE(*str)) {
          if (*obuf->prevchar->ptr != ' ') {
            PUSH(' ');
          }
          str++;
        } else {
          if (*str == '&')
            proc_escape(obuf, &str);
          else
            proc_mchar(obuf, obuf->flag & RB_SPECIAL, delta, &str, mode);
        }
      }
      if (need_flushline(h_env, obuf, mode)) {
        char *bp = obuf->line->ptr + obuf->bp.len;
        char *tp = bp - obuf->bp.tlen;
        int i = 0;

        if (tp > obuf->line->ptr && tp[-1] == ' ')
          i = 1;

        indent = h_env->envs[h_env->envc].indent;
        if (obuf->bp.pos - i > indent) {
          Str line;
          append_tags(obuf); /* may reallocate the buffer */
          bp = obuf->line->ptr + obuf->bp.len;
          line = Strnew_charp(bp);
          Strshrink(obuf->line, obuf->line->length - obuf->bp.len);
          if (obuf->pos - i > h_env->limit)
            obuf->flag |= RB_FILL;
          back_to_breakpoint(obuf);
          flushline(h_env, obuf, indent, 0, h_env->limit);
          obuf->flag &= ~RB_FILL;
          HTMLlineproc1(line->ptr, h_env);
        }
      }
    }
  }
  if (!(obuf->flag & (RB_SPECIAL | RB_INTXTA | RB_INSELECT))) {
    char *tp;
    int i = 0;

    if (obuf->bp.pos == obuf->pos) {
      tp = &obuf->line->ptr[obuf->bp.len - obuf->bp.tlen];
    } else {
      tp = &obuf->line->ptr[obuf->line->length];
    }

    if (tp > obuf->line->ptr && tp[-1] == ' ')
      i = 1;
    indent = h_env->envs[h_env->envc].indent;
    if (obuf->pos - i > h_env->limit) {
      obuf->flag |= RB_FILL;
      flushline(h_env, obuf, indent, 0, h_env->limit);
      obuf->flag &= ~RB_FILL;
    }
  }
}

extern char *NullLine;
extern Lineprop NullProp[];

#define addnewline2(a, b, c, d, e, f) _addnewline2(a, b, c, e, f)
static void addnewline2(struct Buffer *buf, char *line, Lineprop *prop,
                        Linecolor *color, int pos, int nlines) {
  Line *l;
  l = New(Line);
  l->next = NULL;
  l->lineBuf = line;
  l->propBuf = prop;
  l->len = pos;
  l->width = -1;
  l->size = pos;
  l->bpos = 0;
  l->bwidth = 0;
  l->prev = buf->currentLine;
  if (buf->currentLine) {
    l->next = buf->currentLine->next;
    buf->currentLine->next = l;
  } else
    l->next = NULL;
  if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
    buf->lastLine = l;
  buf->currentLine = l;
  if (buf->firstLine == NULL)
    buf->firstLine = l;
  l->linenumber = ++buf->allLine;
  if (nlines < 0) {
    /*     l->real_linenumber = l->linenumber;     */
    l->real_linenumber = 0;
  } else {
    l->real_linenumber = nlines;
  }
  l = NULL;
}

static void addnewline(struct Buffer *buf, char *line, Lineprop *prop,
                       Linecolor *color, int pos, int width, int nlines) {
  char *s;
  Lineprop *p;
  Line *l;
  int i, bpos, bwidth;

  if (pos > 0) {
    s = allocStr(line, pos);
    p = NewAtom_N(Lineprop, pos);
    memcpy((void *)p, (const void *)prop, pos * sizeof(Lineprop));
  } else {
    s = NullLine;
    p = NullProp;
  }
  addnewline2(buf, s, p, c, pos, nlines);
  if (pos <= 0 || width <= 0)
    return;
  bpos = 0;
  bwidth = 0;
  while (1) {
    l = buf->currentLine;
    l->bpos = bpos;
    l->bwidth = bwidth;
    i = columnLen(l, width);
    if (i == 0) {
      i++;
    }
    l->len = i;
    l->width = COLPOS(l, l->len);
    if (pos <= i)
      return;
    bpos += l->len;
    bwidth += l->width;
    s += i;
    p += i;
    pos -= i;
    addnewline2(buf, s, p, c, pos, nlines);
  }
}

/*
 * loadHTMLBuffer: read file and make new buffer
 */
struct Buffer *loadHTMLBuffer(struct URLFile *f, struct Buffer *newBuf) {
  FILE *src = NULL;
  Str tmp;

  if (newBuf == NULL)
    newBuf = newBuffer(INIT_BUFFER_WIDTH);
  if (newBuf->sourcefile == NULL &&
      (f->scheme != SCM_LOCAL || newBuf->mailcap)) {
    tmp = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmp->ptr, "w");
    if (src)
      newBuf->sourcefile = tmp->ptr;
  }

  loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  if (n_textarea)
    formResetBuffer(newBuf, newBuf->formitem);
  if (src)
    fclose(src);

  return newBuf;
}

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

void init_henv(struct html_feed_environ *h_env, struct readbuffer *obuf,
               struct environment *envs, int nenv, TextLineList *buf, int limit,
               int indent) {
  envs[0].indent = indent;

  obuf->line = Strnew();
  obuf->cprop = 0;
  obuf->pos = 0;
  obuf->prevchar = Strnew_size(8);
  set_space_to_prevchar(obuf->prevchar);
  obuf->flag = RB_IGNORE_P;
  obuf->flag_sp = 0;
  obuf->status = R_ST_NORMAL;
  obuf->table_level = -1;
  obuf->nobr_level = 0;
  obuf->q_level = 0;
  memset((void *)&obuf->anchor, 0, sizeof(obuf->anchor));
  obuf->img_alt = 0;
  obuf->input_alt.hseq = 0;
  obuf->input_alt.fid = -1;
  obuf->input_alt.in = 0;
  obuf->input_alt.type = NULL;
  obuf->input_alt.name = NULL;
  obuf->input_alt.value = NULL;
  obuf->in_bold = 0;
  obuf->in_italic = 0;
  obuf->in_under = 0;
  obuf->in_strike = 0;
  obuf->in_ins = 0;
  obuf->prev_ctype = PC_ASCII;
  obuf->tag_sp = 0;
  obuf->fontstat_sp = 0;
  obuf->top_margin = 0;
  obuf->bottom_margin = 0;
  obuf->bp.init_flag = 1;
  set_breakpoint(obuf, 0);

  h_env->buf = buf;
  h_env->f = NULL;
  h_env->obuf = obuf;
  h_env->tagbuf = Strnew();
  h_env->limit = limit;
  h_env->maxlimit = 0;
  h_env->envs = envs;
  h_env->nenv = nenv;
  h_env->envc = 0;
  h_env->envc_real = 0;
  h_env->title = NULL;
  h_env->blank_lines = 0;
}

void completeHTMLstream(struct html_feed_environ *h_env,
                        struct readbuffer *obuf) {
  close_anchor(h_env, obuf);
  if (obuf->img_alt) {
    push_tag(obuf, "</img_alt>", HTML_N_IMG_ALT);
    obuf->img_alt = NULL;
  }
  if (obuf->input_alt.in) {
    push_tag(obuf, "</input_alt>", HTML_N_INPUT_ALT);
    obuf->input_alt.hseq = 0;
    obuf->input_alt.fid = -1;
    obuf->input_alt.in = 0;
    obuf->input_alt.type = NULL;
    obuf->input_alt.name = NULL;
    obuf->input_alt.value = NULL;
  }
  if (obuf->in_bold) {
    push_tag(obuf, "</b>", HTML_N_B);
    obuf->in_bold = 0;
  }
  if (obuf->in_italic) {
    push_tag(obuf, "</i>", HTML_N_I);
    obuf->in_italic = 0;
  }
  if (obuf->in_under) {
    push_tag(obuf, "</u>", HTML_N_U);
    obuf->in_under = 0;
  }
  if (obuf->in_strike) {
    push_tag(obuf, "</s>", HTML_N_S);
    obuf->in_strike = 0;
  }
  if (obuf->in_ins) {
    push_tag(obuf, "</ins>", HTML_N_INS);
    obuf->in_ins = 0;
  }
  if (obuf->flag & RB_INTXTA)
    HTMLlineproc1("</textarea>", h_env);
  /* for unbalanced select tag */
  if (obuf->flag & RB_INSELECT)
    HTMLlineproc1("</select>", h_env);
  if (obuf->flag & RB_TITLE)
    HTMLlineproc1("</title>", h_env);

  /* for unbalanced table tag */
  if (obuf->table_level >= MAX_TABLE)
    obuf->table_level = MAX_TABLE - 1;

  while (obuf->table_level >= 0) {
    int tmp = obuf->table_level;
    table_mode[obuf->table_level].pre_mode &=
        ~(TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN);
    HTMLlineproc1("</table>", h_env);
    if (obuf->table_level >= tmp)
      break;
  }
}

static void print_internal_information(struct html_feed_environ *henv) {
  int i;
  Str s;
  TextLineList *tl = newTextLineList();

  s = Strnew_charp("<internal>");
  pushTextLine(tl, newTextLine(s, 0));
  if (henv->title) {
    s = Strnew_m_charp("<title_alt title=\"", html_quote(henv->title), "\">",
                       NULL);
    pushTextLine(tl, newTextLine(s, 0));
  }
  if (n_textarea > 0) {
    for (i = 0; i < n_textarea; i++) {
      s = Sprintf("<textarea_int textareanumber=%d>", i);
      pushTextLine(tl, newTextLine(s, 0));
      s = Strnew_charp(html_quote(textarea_str[i]->ptr));
      Strcat_charp(s, "</textarea_int>");
      pushTextLine(tl, newTextLine(s, 0));
    }
  }
  s = Strnew_charp("</internal>");
  pushTextLine(tl, newTextLine(s, 0));

  if (henv->buf)
    appendTextLineList(henv->buf, tl);
  else if (henv->f) {
    TextLineListItem *p;
    for (p = tl->first; p; p = p->next)
      fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
  }
}

void loadHTMLstream(struct URLFile *f, struct Buffer *newBuf, FILE *src,
                    int internal) {
  struct environment envs[MAX_ENV_LEVEL];
  int64_t linelen = 0;
  int64_t trbyte = 0;
  Str lineBuf2 = Strnew();
  struct html_feed_environ htmlenv1;
  struct readbuffer obuf;
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;

  symbol_width = symbol_width0 = 1;

  cur_title = NULL;
  n_textarea = 0;
  cur_textarea = NULL;
  max_textarea = MAX_TEXTAREA;
  textarea_str = New_N(Str, max_textarea);
  cur_select = NULL;
  form_sp = -1;
  form_max = -1;
  forms_size = 0;
  forms = NULL;
  cur_hseq = 1;

  if (w3m_halfload) {
    newBuf->buffername = "---";
    max_textarea = 0;
    HTMLlineproc3(newBuf, f->stream);
    w3m_halfload = FALSE;
    return;
  }

  init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

  htmlenv1.buf = newTextLineList();
#if defined(USE_M17N) || defined(USE_IMAGE)
  cur_baseURL = baseURL(newBuf);
#endif

  if (SETJMP(AbortLoading) != 0) {
    HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
    goto phase2;
  }
  TRAP_ON;

  if (IStype(f->stream) != IST_ENCODED)
    f->stream = newEncodedStream(f->stream, f->encoding);
  while ((lineBuf2 = StrmyUFgets(f))->length) {
    if (src)
      Strfputs(lineBuf2, src);
    linelen += lineBuf2->length;
    term_showProgress(&linelen, &trbyte, current_content_length);
    /*
     * if (frame_source)
     * continue;
     */
    lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
    HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
  }
  if (obuf.status != R_ST_NORMAL) {
    HTMLlineproc0("\n", &htmlenv1, internal);
  }
  obuf.status = R_ST_NORMAL;
  completeHTMLstream(&htmlenv1, &obuf);
  flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
#if defined(USE_M17N) || defined(USE_IMAGE)
  cur_baseURL = NULL;
#endif
  if (htmlenv1.title)
    newBuf->buffername = htmlenv1.title;
phase2:
  newBuf->trbyte = trbyte + linelen;
  TRAP_OFF;
  HTMLlineproc2(newBuf, htmlenv1.buf);
}

/*
 * loadHTMLString: read string and make new buffer
 */
struct Buffer *loadHTMLString(Str page) {
  struct URLFile f;
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;
  struct Buffer *newBuf;

  init_stream(&f, SCM_LOCAL, newStrStream(page));

  newBuf = newBuffer(INIT_BUFFER_WIDTH);
  if (SETJMP(AbortLoading) != 0) {
    TRAP_OFF;
    discardBuffer(newBuf);
    UFclose(&f);
    return NULL;
  }
  TRAP_ON;

  loadHTMLstream(&f, newBuf, NULL, TRUE);

  TRAP_OFF;
  UFclose(&f);
  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  newBuf->type = "text/html";
  newBuf->real_type = newBuf->type;
  if (n_textarea)
    formResetBuffer(newBuf, newBuf->formitem);
  return newBuf;
}

/*
 * loadBuffer: read file and make new buffer
 */
struct Buffer *loadBuffer(struct URLFile *uf, struct Buffer *newBuf) {
  FILE *src = NULL;
  Str lineBuf2;
  char pre_lbuf = '\0';
  int nlines;
  Str tmpf;
  int64_t linelen = 0, trbyte = 0;
  Lineprop *propBuffer = NULL;
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;

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
  while ((lineBuf2 = StrmyISgets(uf->stream))->length) {
    if (src)
      Strfputs(lineBuf2, src);
    linelen += lineBuf2->length;
    term_showProgress(&linelen, &trbyte, current_content_length);
    if (frame_source)
      continue;
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

static int _MoveFile(char *path1, char *path2) {
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




