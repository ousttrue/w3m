#include "input/loader.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "core.h"
#include "file/file.h"
#include "file/tmpfile.h"
#include "html/html_readbuffer.h"
#include "input/ftp.h"
#include "input/http_auth.h"
#include "input/http_request.h"
#include "input/http_response.h"
#include "input/istream.h"
#include "input/localcgi.h"
#include "input/url_stream.h"
#include "os.h"
#include "rc.h"
#include "siteconf.h"
#include "term/terms.h"
#include "text/datetime.h"
#include "text/text.h"
#include "trap_jmp.h"
#include <stdio.h>
#include <sys/stat.h>

const char *DefaultType = nullptr;
bool use_proxy = true;
const char *FTP_proxy = nullptr;
bool UseExternalDirBuffer = true;
bool label_topline = false;
bool retryAsHttp = true;
bool AutoUncompress = false;
int FollowRedirection = 10;
const char *DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;

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

static int doFileMove(char *tmpf, char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

static struct Buffer *page_loaded(int cols, struct Url pu, struct URLFile f, Str page,
                                  const char *t, const char *real_type,
                                  struct Buffer *t_buf) {
  if (page) {
    auto tmp = tmpfname(TMPF_SRC, ".html");
    auto src = fopen(tmp->ptr, "w");
    if (src) {
      Strfputs(page, src);
      fclose(src);
    }

    auto doc = loadHTML(cols, page->ptr, pu, nullptr, CHARSET_UNKONWN);
    if (!doc) {
      return nullptr;
    }

    // copyParsedURL(&b->currentURL, &pu);
    // b->real_scheme = pu.scheme;
    // b->real_type = real_type;
    // if (src)
    //   b->sourcefile = tmp->ptr;
    auto b = newBuffer();
    b->document = doc;
    return b;
  }

  // if (real_type == NULL)
  //   real_type = t;

  f.current_content_length = 0;
  const char *p;
  if ((p = httpGetHeader(t_buf->http_response, "Content-Length:")) != NULL)
    f.current_content_length = strtoclen(p);

  if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    uncompress_stream(&f, &pu.real_file);
  } else if (f.compression != CMP_NOCOMPRESS) {
    if (is_text_type(t)) {
      if (t_buf == NULL)
        t_buf = newBuffer();
      uncompress_stream(&f, &t_buf->sourcefile);
      uncompressed_file_type(pu.file, &f.ext);
    } else {
      t = compress_application_type(f.compression);
      f.compression = CMP_NOCOMPRESS;
    }
  }

  if (t_buf == NULL)
    t_buf = newBuffer();
  copyParsedURL(&t_buf->currentURL, &pu);
  t_buf->filename = pu.real_file ? pu.real_file : pu.file;
  t_buf->ssl_certificate = f.ssl_certificate;

  FILE *src = NULL;
  if (f.scheme != SCM_LOCAL) {
    auto tmp = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmp->ptr, "w");
    if (src) {
      t_buf->sourcefile = tmp->ptr;
    }
  }
  Str content = Strnew();
  Str line;
  while ((line = StrmyUFgets(&f))->length) {
    Strcat(content, line);
  }
  if (src) {
    Strfputs(content, src);
    fclose(src);
  }

  struct Buffer *b;
  if (is_html_type(t)) {
    b = t_buf;
    b->document = loadHTML(cols, content->ptr, t_buf->currentURL, baseURL(t_buf),
                           t_buf->http_response->content_charset);
  } else {
    b = t_buf;
    b->document = loadText(cols, content->ptr);
  }

  if (b != NULL) {
    if (b->buffername == NULL || b->buffername[0] == '\0') {
      b->buffername = httpGetHeader(b->http_response, "Subject:");
      if (b->buffername == NULL && b->filename != NULL)
        b->buffername = lastFileName(b->filename);
    }
    if (b->currentURL.scheme == SCM_UNKNOWN)
      b->currentURL.scheme = f.scheme;
    if (f.scheme == SCM_LOCAL && b->sourcefile == NULL)
      b->sourcefile = b->filename;
    if (is_html_type(t))
      b->type = "text/html";
    else
      b->type = "text/plain";
  }

  UFclose(&f);
  if (b && b != NO_BUFFER) {
    b->real_scheme = f.scheme;
    b->real_type = real_type;
    if (pu.label) {
      if (is_html_type(t)) {
        struct Anchor *a;
        a = searchURLLabel(b->document, pu.label);
        if (a != NULL) {
          gotoLine(b->document, a->start.line);
          if (label_topline)
            b->document->topLine =
                lineSkip(b->document, b->document->topLine,
                         b->document->currentLine->linenumber -
                             b->document->topLine->linenumber,
                         false);
          b->document->viewport.pos = a->start.pos;
          arrangeCursor(b->document);
        }
      } else { /* plain text */
        int l = atoi(pu.label);
        gotoRealLine(b, l);
        b->document->viewport.pos = 0;
        arrangeCursor(b->document);
      }
    }
  }
  if (header_string)
    header_string = NULL;
  if (b && b != NO_BUFFER)
    preFormUpdateBuffer(b);
  trap_off();
  return b;
}

static struct Buffer *
load_doc(int cols, const char *path, const char *tpath, struct Url *current,
         struct Url pu, const char *referer, enum RG_FLAGS flag,
         struct FormList *request, struct TextList *extra_header,
         struct URLFile *of, struct HttpRequest hr, enum HttpStatus status,
         bool add_auth_cookie_flag, struct Buffer *b, struct Buffer *t_buf,
         Str realm, Str uname, Str pwd) {
  {
    parseURL2(tpath, &pu, current);
    auto sc_redirect = query_SCONF_SUBSTITUTE_URL(&pu);
    if (sc_redirect && *sc_redirect && checkRedirection(&pu)) {
      tpath = (char *)sc_redirect;
      request = NULL;
      add_auth_cookie_flag = 0;
      current = New(struct Url);
      *current = pu;
      status = HTST_NORMAL;
      return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, realm, uname, pwd);
    }
  }

  trap_off();
  struct URLOption url_option;
  url_option.referer = referer;
  url_option.flag = flag;
  auto f = openURL(tpath, &pu, current, &url_option, request, extra_header, of,
                   &hr, &status);
  if (f.stream == NULL && retryAsHttp && tpath[0] != '/') {
    auto u = tpath;
    auto scheme = getURLScheme(&u);
    if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
      // retry it as "http://"
      u = Strnew_m_charp("http://", tpath, NULL)->ptr;
      auto f = openURL(u, &pu, current, &url_option, request, extra_header, of,
                       &hr, &status);
    }
  }

  Str page = nullptr;
  of = NULL;
  const char *t = "text/plain";
  const char *real_type = nullptr;
  if (f.stream == NULL) {
    switch (f.scheme) {
    case SCM_LOCAL: {
      struct stat st;
      if (stat(pu.real_file, &st) < 0)
        return NULL;
      if (S_ISDIR(st.st_mode)) {
        if (UseExternalDirBuffer) {
          Str cmd = Sprintf("%s?dir=%s#current", DirBufferCommand, pu.file);
          auto b = loadGeneralFile(cols, cmd->ptr, NULL, NO_REFERER, 0, NULL);
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
      return page_loaded(cols, pu, f, page, t, real_type, t_buf);
    return NULL;
  }

  if (status == HTST_MISSING) {
    trap_off();
    UFclose(&f);
    return NULL;
  }

  /* openURL() succeeded */
  if (from_jmp()) {
    /* transfer interrupted */
    trap_off();
    if (b)
      discardBuffer(b);
    UFclose(&f);
    return NULL;
  }

  b = NULL;
  if (f.is_cgi) {
    /* local CGI */
  }
  if (header_string)
    header_string = NULL;
  trap_on();
  if (pu.scheme == SCM_HTTP || pu.scheme == SCM_HTTPS ||
      (((pu.scheme == SCM_FTP && non_null(FTP_proxy))) && use_proxy &&
       !check_no_proxy(pu.host))) {

    term_message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr);
    if (t_buf == NULL)
      t_buf = newBuffer();

    t_buf->http_response = httpReadResponse(&f, &pu);
    const char *p;
    if (((t_buf->http_response->http_status_code >= 301 &&
          t_buf->http_response->http_status_code <= 303) ||
         t_buf->http_response->http_status_code == 307) &&
        (p = httpGetHeader(t_buf->http_response, "Location:")) != NULL &&
        checkRedirection(&pu)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      tpath = url_quote(p);
      request = NULL;
      UFclose(&f);
      current = New(struct Url);
      copyParsedURL(current, &pu);
      t_buf = newBuffer();
      t_buf->bufferprop |= BP_REDIRECTED;
      status = HTST_NORMAL;
      return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, realm, uname, pwd);
    }

    t = httpGetContentType(t_buf->http_response);
    if (t == NULL && pu.file != NULL) {
      if (!((t_buf->http_response->http_status_code >= 400 &&
             t_buf->http_response->http_status_code <= 407) ||
            (t_buf->http_response->http_status_code >= 500 &&
             t_buf->http_response->http_status_code <= 505)))
        t = guessContentType(pu.file);
    }
    if (t == NULL)
      t = "text/plain";

    if (add_auth_cookie_flag && realm && uname && pwd) {
      /* If authorization is required and passed */
      add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd, 0);
      add_auth_cookie_flag = 0;
    }
    if ((p = httpGetHeader(t_buf->http_response, "WWW-Authenticate:")) !=
            NULL &&
        t_buf->http_response->http_status_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auto auth_pu = &pu;
        getAuthCookie(&hauth, "Authorization:", extra_header, auth_pu, &hr,
                      request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          trap_off();
          return page_loaded(cols, pu, f, page, t, real_type, t_buf);
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                        extra_header, of, hr, status, add_auth_cookie_flag, b,
                        t_buf, realm, uname, pwd);
      }
    }
    if ((p = httpGetHeader(t_buf->http_response, "Proxy-Authenticate:")) !=
            NULL &&
        t_buf->http_response->http_status_code == 407) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auto auth_pu = schemeToProxy(pu.scheme);
        getAuthCookie(&hauth, "Proxy-Authorization:", extra_header, auth_pu,
                      &hr, request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          trap_off();
          return page_loaded(cols, pu, f, page, t, real_type, t_buf);
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                        extra_header, of, hr, status, add_auth_cookie_flag, b,
                        t_buf, realm, uname, pwd);
      }
    }
    /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

    if (status == HTST_CONNECT) {
      of = &f;
      return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, realm, uname, pwd);
    }

    f.modtime = mymktime(httpGetHeader(t_buf->http_response, "Last-Modified:"));
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

  return page_loaded(cols, pu, f, page, t, real_type, t_buf);
}

struct Buffer *loadGeneralFile(int cols, const char *path, struct Url *current,
                               const char *referer, enum RG_FLAGS flag,
                               struct FormList *request) {
  checkRedirection(NULL);

  struct Url pu;
  struct Buffer *b = NULL;
  struct Buffer *t_buf = NULL;
  struct TextList *extra_header = newTextList();
  Str uname = NULL;
  Str pwd = NULL;
  Str realm = NULL;
  unsigned char status = HTST_NORMAL;
  struct HttpRequest hr;
  auto tpath = path;
  bool add_auth_cookie_flag = 0;
  return load_doc(cols, path, tpath, current, pu, referer, flag, request,
                  extra_header, nullptr, hr, status, add_auth_cookie_flag, b,
                  t_buf, realm, uname, pwd);
}

struct Buffer *loadcmdout(const char *cmd, LoadProc loadproc,
                          struct Buffer *defaultbuf) {

  if (cmd == NULL || *cmd == '\0')
    return NULL;

  auto f = popen(cmd, "r");
  if (!f)
    return NULL;

  struct URLFile uf;
  init_stream(&uf, SCM_UNKNOWN, newFileStream(f, (void (*)())pclose));
  auto buf = loadproc(&uf, nullptr, defaultbuf);
  UFclose(&uf);
  return buf;
}
