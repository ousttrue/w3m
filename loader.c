#include "loader.h"
#include "trap_jmp.h"
#include "filepath.h"
#include "datetime.h"
#include "mailcap.h"
#include "os.h"
#include "app.h"
#include "termsize.h"
#include "istream.h"
#include "indep.h"
#include "alloc.h"
#include "url.h"
#include "url_stream.h"
#include "form.h"
#include "buffer.h"
#include "readbuffer.h"
#include "http_request.h"
#include "http_response.h"
#include "http_auth.h"
#include "terms.h"
#include "fm.h"
#include "etc.h"
#include "file.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <stdio.h>

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

#define DO_EXTERNAL                                                            \
  ((struct Buffer * (*)(struct URLFile *, struct Buffer *)) doExternal)

static struct Buffer *page_loaded(struct Url pu, struct URLFile f, Str page,
                                  const char *t, const char *real_type,
                                  struct Buffer *t_buf) {
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;
  if (page) {
    auto tmp = tmpfname(TMPF_SRC, ".html");
    auto src = fopen(tmp->ptr, "w");
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
    auto b = loadHTMLString(page);
    if (b) {
      copyParsedURL(&b->currentURL, &pu);
      b->real_scheme = pu.scheme;
      b->real_type = real_type;
      if (src)
        b->sourcefile = tmp->ptr;
    }
    return b;
  }

  // if (real_type == NULL)
  //   real_type = t;
  auto proc = loadBuffer;

  f.current_content_length = 0;
  char *p;
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
  struct Buffer *b;
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

struct Buffer *load_doc(char *path, char *tpath, struct Url *current,
                        struct Url pu, char *referer, enum RG_FLAGS flag,
                        struct FormList *request, struct TextList *extra_header,
                        struct URLFile *of, struct HttpRequest hr,
                        enum HttpStatus status, bool add_auth_cookie_flag,
                        struct Buffer *b, struct Buffer *t_buf,
                        bool searchHeader, bool searchHeader_through, Str realm,
                        Str uname, Str pwd) {
  MySignalHandler (*prevtrap)(SIGNAL_ARG) = NULL;

  {
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
      return load_doc(path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, searchHeader, searchHeader_through, realm, uname,
                      pwd);
    }
  }

  TRAP_OFF;
  struct URLOption url_option;
  url_option.referer = referer;
  url_option.flag = flag;
  auto f = openURL(tpath, &pu, current, &url_option, request, extra_header, of,
                   &hr, &status);
  // if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
  //   if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
  //     /* retry it as "http://" */
  //     u = Strnew_m_charp("http://", url, NULL)->ptr;
  //     goto retry;
  //   }
  // }

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
          auto b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0, NULL);
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
      return page_loaded(pu, f, page, t, real_type, t_buf);
    return NULL;
  }

  if (status == HTST_MISSING) {
    TRAP_OFF;
    UFclose(&f);
    return NULL;
  }

  /* openURL() succeeded */
  if (from_jmp()) {
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
  trap_on();
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
    char *p;
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
      return load_doc(path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, searchHeader, searchHeader_through, realm, uname,
                      pwd);
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
        auto auth_pu = &pu;
        getAuthCookie(&hauth, "Authorization:", extra_header, auth_pu, &hr,
                      request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          TRAP_OFF;
          return page_loaded(pu, f, page, t, real_type, t_buf);
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        return load_doc(path, tpath, current, pu, referer, flag, request,
                        extra_header, of, hr, status, add_auth_cookie_flag, b,
                        t_buf, searchHeader, searchHeader_through, realm, uname,
                        pwd);
      }
    }
    if ((p = checkHeader(t_buf, "Proxy-Authenticate:")) != NULL &&
        http_response_code == 407) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:") != NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auto auth_pu = schemeToProxy(pu.scheme);
        getAuthCookie(&hauth, "Proxy-Authorization:", extra_header, auth_pu,
                      &hr, request, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          TRAP_OFF;
          return page_loaded(pu, f, page, t, real_type, t_buf);
        }
        UFclose(&f);
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        return load_doc(path, tpath, current, pu, referer, flag, request,
                        extra_header, of, hr, status, add_auth_cookie_flag, b,
                        t_buf, searchHeader, searchHeader_through, realm, uname,
                        pwd);
      }
    }
    /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

    if (status == HTST_CONNECT) {
      of = &f;
      return load_doc(path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, searchHeader, searchHeader_through, realm, uname,
                      pwd);
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
    char *p;
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
      return load_doc(path, tpath, current, pu, referer, flag, request,
                      extra_header, of, hr, status, add_auth_cookie_flag, b,
                      t_buf, searchHeader, searchHeader_through, realm, uname,
                      pwd);
    }
    // #ifdef AUTH_DEBUG
    //     if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL) {
    //       /* Authentication needed */
    //       struct http_auth hauth;
    //       if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL
    //       &&
    //           (realm = get_auth_param(hauth.param, "realm")) != NULL) {
    //         auth_pu = &pu;
    //         getAuthCookie(&hauth, "Authorization:", extra_header, auth_pu,
    //         &hr,
    //                       request, &uname, &pwd);
    //         if (uname == NULL) {
    //           /* abort */
    //           TRAP_OFF;
    //           goto page_loaded;
    //         }
    //         UFclose(&f);
    //         add_auth_cookie_flag = 1;
    //         status = HTST_NORMAL;
    //         goto load_doc;
    //       }
    //     }
    // #endif /* defined(AUTH_DEBUG) */
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

  return page_loaded(pu, f, page, t, real_type, t_buf);
}

struct Buffer *loadGeneralFile(char *path, struct Url *current, char *referer,
                               enum RG_FLAGS flag, struct FormList *request) {
  checkRedirection(NULL);

  struct Url pu;
  struct Buffer *b = NULL;
  struct Buffer *t_buf = NULL;
  int searchHeader = SearchHeader;
  int searchHeader_through = true;
  struct TextList *extra_header = newTextList();
  Str uname = NULL;
  Str pwd = NULL;
  Str realm = NULL;
  unsigned char status = HTST_NORMAL;
  struct HttpRequest hr;
  auto tpath = path;
  bool add_auth_cookie_flag = 0;
  return load_doc(path, tpath, current, pu, referer, flag, request,
                  extra_header, nullptr, hr, status, add_auth_cookie_flag, b,
                  t_buf, searchHeader, searchHeader_through, realm, uname, pwd);
}

struct Buffer *loadcmdout(char *cmd,
                          struct Buffer *(*loadproc)(struct URLFile *,
                                                     struct Buffer *),
                          struct Buffer *defaultbuf) {

  if (cmd == NULL || *cmd == '\0')
    return NULL;

  auto f = popen(cmd, "r");
  if (!f)
    return NULL;

  struct URLFile uf;
  init_stream(&uf, SCM_UNKNOWN, newFileStream(f, (void (*)())pclose));
  auto buf = loadproc(&uf, defaultbuf);
  UFclose(&uf);
  return buf;
}
