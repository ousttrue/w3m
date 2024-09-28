#include "file.h"
#include "mailcap.h"
#include "http_response.h"
#include "url.h"
#include "istream.h"
#include "compression.h"
#include "app.h"
#include "indep.h"
#include "alloc.h"
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
#include "http_auth.h"
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

#define SAVE_BUF_SIZE 1536

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
    return true;
  }
  if (nredir >= FollowRedirection) {
    /* FIXME: gettextize? */
    tmp = Sprintf("Number of redirections exceeded %d at %s", FollowRedirection,
                  parsedURL2Str(pu)->ptr);
    disp_err_message(tmp->ptr, false);
    return false;
  } else if (nredir_size > 0 &&
             (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) ||
              (!(nredir % 2) &&
               same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    /* FIXME: gettextize? */
    tmp = Sprintf("Redirection loop detected (%s)", parsedURL2Str(pu)->ptr);
    disp_err_message(tmp->ptr, false);
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
  const char *t = "text/plain";
  char *p;
  const char *real_type = NULL;
  struct Buffer *t_buf = NULL;
  int searchHeader = SearchHeader;
  int searchHeader_through = true;
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
  // if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
  //   if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
  //     /* retry it as "http://" */
  //     u = Strnew_m_charp("http://", url, NULL)->ptr;
  //     goto retry;
  //   }
  // }

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
                       false);
      break;

    default:
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
    searchHeader = true;
    searchHeader_through = false;
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
    auto http_response_code = readHeader(&f, t_buf, false, &pu);
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
      auto t1 = uncompressed_file_type(pu.file, NULL);
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
    searchHeader = SearchHeader = false;
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

  f.current_content_length = 0;
  if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
    f.current_content_length = strtoclen(p);
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
    if (is_text_type(t) || searchExtViewer((char *)t)) {
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
    if (!do_download && searchExtViewer((char *)t) != NULL) {
      proc = DO_EXTERNAL;
    } else {
      TRAP_OFF;
      if (pu.scheme == SCM_LOCAL) {
        UFclose(&f);
        _doFileCopy(pu.real_file,
                    conv_from_system(guess_save_name(NULL, pu.real_file)),
                    true);
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
    b = doExternal(f, (char *)t, t_buf);
  } else {
    b = loadSomething(&f, proc, t_buf);
  }
  UFclose(&f);
  if (b && b != NO_BUFFER) {
    b->real_scheme = f.scheme;
    b->real_type = real_type;
    if (pu.label) {
      if (proc == loadHTMLBuffer) {
        struct Anchor *a;
        a = searchURLLabel(b, pu.label);
        if (a != NULL) {
          gotoLine(b, a->start.line);
          if (label_topline)
            b->topLine = lineSkip(
                b, b->topLine,
                b->currentLine->linenumber - b->topLine->linenumber, false);
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
  int is_html = false;

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
      term_showProgress(&linelen, &trbyte, uf.current_content_length);
    }
  }
_end:
  memcpy(AbortLoading, env_bak, sizeof(JMP_BUF));
  TRAP_OFF;
  xfree(buf);
  fclose(ff);
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

int checkSaveFile(union input_stream *stream, char *path2) {
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
  auto f1 = openIS(path1);
  if (!f1)
    return -1;

  FILE *f2;
  bool is_pipe;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = true;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = false;
    f2 = fopen(path2, "wb");
  }
  if (!f2) {
    ISclose(f1);
    return -1;
  }

  int64_t linelen = 0;
  int64_t trbyte = 0;
  int count;
  uint64_t current_content_length = 0;
  auto buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
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
