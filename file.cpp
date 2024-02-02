#include "file.h"
#include "app.h"
#include "httpauth.h"
#include "mytime.h"
#include "func.h"
#include "alloc.h"
#include "indep.h"
#include "compression.h"
#include "url_stream.h"
#include "message.h"
#include "mimetypes.h"
#include "screen.h"
#include "history.h"
#include "w3m.h"
#include "url.h"
#include "terms.h"
#include "tmpfile.h"
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
#include "table.h"
#include "mailcap.h"
#include "signal_util.h"
#include "myctype.h"
#include "html.h"
#include "htmltag.h"
#include "local_cgi.h"
#include "regex.h"
#include <sys/types.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"

bool SearchHeader = false;
int FollowRedirection = 10;
const char *DefaultType = nullptr;
bool DecodeCTE = false;
#define DEF_SAVE_FILE "index.html"

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
}

static const char *guess_filename(const char *file);
static Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(UrlStream *, Buffer *),
                          Buffer *defaultbuf);

static int http_response_code;

int currentLn(Buffer *buf) {
  if (buf->currentLine)
    /*     return buf->currentLine->real_linenumber + 1;      */
    return buf->currentLine->linenumber + 1;
  else
    return 1;
}

static Buffer *loadSomething(UrlStream *f,
                             Buffer *(*loadproc)(UrlStream *, Buffer *),
                             Buffer *defaultbuf) {
  Buffer *buf;

  if ((buf = loadproc(f, defaultbuf)) == NULL)
    return NULL;

  if (buf->buffername == NULL || buf->buffername[0] == '\0') {
    buf->buffername = checkHeader(buf, "Subject:");
    if (buf->buffername == NULL && buf->filename != NULL)
      buf->buffername = lastFileName(buf->filename);
  }
  if (buf->currentURL.schema == SCM_UNKNOWN)
    buf->currentURL.schema = f->schema;
  if (f->schema == SCM_LOCAL && buf->sourcefile == NULL)
    buf->sourcefile = buf->filename;
  if (loadproc == loadHTMLBuffer)
    buf->type = "text/html";
  else
    buf->type = "text/plain";
  return buf;
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

int setModtime(const char *path, time_t modtime) {
  struct utimbuf t;
  struct stat st;

  if (stat(path, &st) == 0)
    t.actime = st.st_atime;
  else
    t.actime = time(NULL);
  t.modtime = modtime;
  return utime(path, &t);
}

/*
 * convert line
 */
Str *convertLine0(UrlStream *uf, Str *line, int mode) {
  if (mode != RAW_MODE)
    cleanup_line(line, mode);
  return line;
}

void readHeader(UrlStream *uf, Buffer *newBuf, int thru, ParsedURL *pu) {
  char *p, *q;
  char c;
  Str *lineBuf2 = NULL;
  Str *tmp;
  TextList *headerlist;
  char *tmpf;
  FILE *src = NULL;
  Lineprop *propBuffer;

  headerlist = newBuf->document_header = newTextList();
  if (uf->schema == SCM_HTTP || uf->schema == SCM_HTTPS)
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
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
        Strcat(tmp, lineBuf2);
        if (thru)
          newBuf->addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length,
                             FOLD_BUFFER_WIDTH, -1);
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

    } else if (use_cookie && accept_cookie && pu &&
               check_cookie_accept_domain(pu->host) &&
               (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {

      process_http_cookie(pu, lineBuf2);

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
    if (headerlist)
      pushText(headerlist, lineBuf2->ptr);
    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
  if (thru)
    newBuf->addnewline("", propBuffer, 0, -1, -1);
  if (src)
    fclose(src);
}

const char *checkHeader(Buffer *buf, const char *field) {
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
  Str *r;
  auto p = checkHeader(buf, "Content-Type:");
  if (p == NULL)
    return NULL;
  r = Strnew();
  while (*p && *p != ';' && !IS_SPACE(*p))
    Strcat_char(r, *p++);
  return r->ptr;
}

static int same_url_p(ParsedURL *pu1, ParsedURL *pu2) {
  return (pu1->schema == pu2->schema && pu1->port == pu2->port &&
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
#define DO_EXTERNAL ((Buffer * (*)(UrlStream *, Buffer *)) doExternal)
Buffer *loadGeneralFile(const char *path, ParsedURL *current,
                        const char *referer, int flag, FormList *request) {
  UrlStream f, *of = NULL;
  ParsedURL pu;
  Buffer *b = NULL;
  Buffer *(*proc)(UrlStream *, Buffer *) = loadBuffer;
  const char *tpath;
  const char *t = "text/plain";
  const char *p = NULL;
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
  url_option.referer = (char *)referer;
  url_option.flag = flag;
  f = openURL((char *)tpath, &pu, current, &url_option, request, extra_header,
              of, &hr, &status);
  of = NULL;
  if (f.stream == NULL) {
    switch (f.schema) {
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
      disp_err_message(Sprintf("Unknown URI: %s", parsedURL2Str(&pu)->ptr)->ptr,
                       FALSE);
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
    searchHeader = TRUE;
    searchHeader_through = FALSE;
  }
  if (header_string)
    header_string = NULL;
  TRAP_ON;
  if (pu.schema == SCM_HTTP || pu.schema == SCM_HTTPS ||
      (((pu.schema == SCM_FTP && non_null(FTP_proxy))) && use_proxy &&
       !check_no_proxy(pu.host))) {

    if (fmInitialized) {
      term_cbreak();
      message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr, 0,
              0);
      refresh(term_io());
    }
    if (t_buf == NULL)
      t_buf = new Buffer(INIT_BUFFER_WIDTH);
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
      tpath = url_quote((char *)p);
      request = NULL;
      UFclose(&f);
      current = (ParsedURL *)New(ParsedURL);
      copyParsedURL(current, &pu);
      t_buf = new Buffer(INIT_BUFFER_WIDTH);
      t_buf->bufferprop =
          static_cast<BufferFlags>(t_buf->bufferprop | BP_REDIRECTED);
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
        auth_pu = schemaToProxy(pu.schema);
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
  } else if (pu.schema == SCM_FTP) {
    check_compression((char *)path, &f);
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
  } else if (pu.schema == SCM_DATA) {
    t = f.guess_type;
  } else if (searchHeader) {
    searchHeader = SearchHeader = FALSE;
    if (t_buf == NULL)
      t_buf = new Buffer(INIT_BUFFER_WIDTH);
    readHeader(&f, t_buf, searchHeader_through, &pu);
    if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL &&
        checkRedirection(&pu)) {
      /* document moved */
      tpath = url_quote(remove_space((char *)p));
      request = NULL;
      UFclose(&f);
      add_auth_cookie_flag = 0;
      current = (ParsedURL *)New(ParsedURL);
      copyParsedURL(current, &pu);
      t_buf = new Buffer(INIT_BUFFER_WIDTH);
      t_buf->bufferprop = (BufferFlags)(t_buf->bufferprop | BP_REDIRECTED);
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
      s = page;
      Strfputs(s, src);
      fclose(src);
    }
    if (do_download) {
      if (!src)
        return NULL;
      auto file = guess_filename(pu.file);
      doFileMove(tmp->ptr, file);
      return NO_BUFFER;
    }
    b = loadHTMLString(page);
    if (b) {
      copyParsedURL(&b->currentURL, &pu);
      b->real_schema = pu.schema;
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
    const char *file;
    TRAP_OFF;
    if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
      f.stream = newEncodedStream(f.stream, f.encoding);
    if (pu.schema == SCM_LOCAL) {
      struct stat st;
      if (PreserveTimestamp && !stat(pu.real_file, &st))
        f.modtime = st.st_mtime;
      file = guess_save_name(NULL, (char *)pu.real_file);
    } else
      file = guess_save_name(t_buf, pu.file);
    if (doFileSave(&f, file) == 0)
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
        t_buf = new Buffer(INIT_BUFFER_WIDTH);
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
      if (pu.schema == SCM_LOCAL) {
        UFclose(&f);
        _doFileCopy(pu.real_file, guess_save_name(NULL, (char *)pu.real_file),
                    TRUE);
      } else {
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
          f.stream = newEncodedStream(f.stream, f.encoding);
        if (doFileSave(&f, guess_save_name(t_buf, pu.file)) == 0)
          UFhalfclose(&f);
        else
          UFclose(&f);
      }
      return NO_BUFFER;
    }
  }
  if (t_buf == NULL)
    t_buf = new Buffer(INIT_BUFFER_WIDTH);
  copyParsedURL(&t_buf->currentURL, &pu);
  t_buf->filename = pu.real_file ? pu.real_file : pu.file;
  t_buf->ssl_certificate = (char *)f.ssl_certificate;
  if (proc == DO_EXTERNAL) {
    b = doExternal(f, t, t_buf);
  } else {
    b = loadSomething(&f, proc, t_buf);
  }
  UFclose(&f);
  if (b && b != NO_BUFFER) {
    b->real_schema = f.schema;
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
Buffer *loadBuffer(UrlStream *uf, Buffer *newBuf) {
  FILE *src = NULL;
  Str *lineBuf2;
  char pre_lbuf = '\0';
  int nlines;
  Str *tmpf;
  long long linelen = 0, trbyte = 0;
  Lineprop *propBuffer = NULL;
  MySignalHandler prevtrap = NULL;

  if (newBuf == NULL)
    newBuf = new Buffer(INIT_BUFFER_WIDTH);

  if (SETJMP(AbortLoading) != 0) {
    goto _end;
  }
  TRAP_ON;

  if (newBuf->sourcefile == NULL &&
      (uf->schema != SCM_LOCAL || newBuf->mailcap)) {
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
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    newBuf->addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length,
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

  auto is_html = is_html_type(buf->type);

  for (; l != NULL; l = l->next) {
    Str *tmp;
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf, l->len);
    Strfputs(tmp, f);
    if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
      putc('\n', f);
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

static Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(UrlStream *, Buffer *),
                          Buffer *defaultbuf) {
  FILE *f, *popen(const char *, const char *);
  Buffer *buf;
  UrlStream uf;

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
Buffer *getshell(const char *cmd) {
  Buffer *buf;

  buf = loadcmdout((char *)cmd, loadBuffer, NULL);
  if (buf == NULL)
    return NULL;
  buf->filename = cmd;
  buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

/*
 * Open pager buffer
 */
Buffer *openPagerBuffer(Buffer *buf) {

  if (buf == NULL)
    buf = new Buffer(INIT_BUFFER_WIDTH);
  buf->buffername = getenv("MAN_PN");
  if (buf->buffername == NULL)
    buf->buffername = PIPEBUFFERNAME;
  buf->bufferprop = (BufferFlags)(buf->bufferprop | BP_PIPE);
  buf->currentLine = buf->firstLine;

  return buf;
}

Buffer *openGeneralPagerBuffer(input_stream *stream) {
  Buffer *buf;
  const char *t = "text/plain";
  Buffer *t_buf = NULL;
  UrlStream uf;

  init_stream(&uf, SCM_UNKNOWN, stream);

  t_buf = new Buffer(INIT_BUFFER_WIDTH);
  copyParsedURL(&t_buf->currentURL, NULL);
  t_buf->currentURL.schema = SCM_LOCAL;
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
    buf = openPagerBuffer(t_buf);
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
      buf = openPagerBuffer(t_buf);
      buf->type = "text/plain";
    }
  }
  buf->real_type = t;
  return buf;
}

Buffer *doExternal(UrlStream uf, const char *type, Buffer *defaultbuf) {
  Str *tmpf, *command;
  struct mailcap *mcap;
  int mc_stat;
  Buffer *buf = NULL;
  const char *header, *src = NULL, *ext = uf.ext;

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
  command =
      unquote_mailcap(mcap->viewer, type, tmpf->ptr, (char *)header, &mc_stat);
  if (!(mc_stat & MCSTAT_REPNAME)) {
    Str *tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
    command = tmp;
  }

  if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) &&
      !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer) {
    flush_tty();
    if (!fork()) {
      setup_child(FALSE, 0, UFfileno(&uf));
      if (save2tmp(&uf, tmpf->ptr) < 0)
        exit(1);
      UFclose(&uf);
      myExec(command->ptr);
    }
    return NO_BUFFER;
  } else {
    if (save2tmp(&uf, tmpf->ptr) < 0) {
      return NULL;
    }
  }
  if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
    if (defaultbuf == NULL)
      defaultbuf = new Buffer(INIT_BUFFER_WIDTH);
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
      buf->mailcap_source = (char *)buf->sourcefile;
      buf->sourcefile = (char *)src;
    }
  } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
    buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->type = "text/plain";
      buf->mailcap_source = (char *)buf->sourcefile;
      buf->sourcefile = (char *)src;
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

static int _MoveFile(const char *path1, const char *path2) {
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

int _doFileCopy(const char *tmpf, const char *defstr, int download) {
  Str *msg;
  Str *filen;
  const char *p, *q = NULL;
  pid_t pid;
  char *lock;
  struct stat st;
  long long size = 0;
  int is_pipe = FALSE;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      // q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
      //                   SaveHist);
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
      p = expandPath((char *)p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      msg = Sprintf("Can't copy. %s and %s are identical.", tmpf, p);
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        msg = Sprintf("Can't save to %s", p);
        disp_err_message(msg->ptr, FALSE);
      }
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    symlink(p, lock);
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
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(char *)(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = q;
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = TRUE;
    else {
      p = expandPath((char *)p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      printf("Can't copy. %s and %s are identical.", tmpf, p);
      return -1;
    }
    if (_MoveFile(tmpf, p) < 0) {
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
      setModtime(p, st.st_mtime);
  }
  return 0;
}

int doFileMove(const char *tmpf, const char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

int checkCopyFile(const char *path1, const char *path2) {
  struct stat st1, st2;

  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
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

static const char *guess_filename(const char *file) {
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

const char *guess_save_name(Buffer *buf, const char *path) {
  if (buf && buf->document_header) {
    Str *name = NULL;
    const char *p, *q;
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
  return (char *)guess_filename(path);
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
