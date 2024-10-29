#include "input/loader.h"
#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/message.h"
#include "file/tmpfile.h"
#include "html/html_readbuffer.h"
#include "input/ftp.h"
#include "input/http_auth.h"
#include "input/http_request.h"
#include "input/http_response.h"
#include "input/istream.h"
#include "input/localcgi.h"
#include "input/proxy.h"
#include "os.h"
#include "rc.h"
#include "siteconf.h"
#include "term/terms.h"
#include "text/text.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>

const char *DefaultType = nullptr;
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
    message_push(tmp->ptr);
    return false;
  } else if (nredir_size > 0 &&
             (same_url_p(pu, &puv[(nredir - 1) % nredir_size]) ||
              (!(nredir % 2) &&
               same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    /* FIXME: gettextize? */
    tmp = Sprintf("Redirection loop detected (%s)", parsedURL2Str(pu)->ptr);
    message_push(tmp->ptr);
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

static struct Document *get_document(int cols,
                                     struct HttpResponse *http_response,
                                     struct Url *currentURL, Str content,
                                     const char *t) {

  // f.current_content_length = 0;
  // const char *p;
  // if ((p = httpGetHeader(t_buf->http_response, "Content-Length:")) != NULL)
  //   f.current_content_length = strtoclen(p);

  // if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
  //   uncompress_stream(&f, &pu.real_file);
  // } else if (f.compression != CMP_NOCOMPRESS) {
  //   if (is_text_type(t)) {
  //     if (t_buf == NULL)
  //       t_buf = newBuffer();
  //     uncompress_stream(&f, &t_buf->sourcefile);
  //     uncompressed_file_type(pu.file, &f.ext);
  //   } else {
  //     t = compress_application_type(f.compression);
  //     f.compression = CMP_NOCOMPRESS;
  //   }
  // }

  struct Document *document;
  if (is_html_type(t)) {
    document = loadHTML(cols, content->ptr, http_response->request->url,
                        currentURL, http_response->content_charset);
  } else {
    document = loadText(cols, content->ptr);
  }

  return document;
}

static struct HttpResponse *sendHttpRequest(struct HttpRequest *req,
                                            union input_stream *of,
                                            bool add_auth_cookie_flag,
                                            Str realm, Str uname, Str pwd) {
  auto sc_redirect = query_SCONF_SUBSTITUTE_URL(&req->url);
  if (sc_redirect && *sc_redirect && checkRedirection(&req->url)) {
    struct Url url;
    parseURL2(sc_redirect, &url, &req->url);
    add_auth_cookie_flag = 0;
    req = newHttpRequest(url, nullptr, req->referer, req->no_cache,
                         req->extra_header);
    return sendHttpRequest(req, of, add_auth_cookie_flag, realm, uname, pwd);
  }

  auto http_response = openURL(req, of);
  if ((!http_response || http_response->stream == NULL) && retryAsHttp &&
      req->url.file[0] != '/') {
    if (req->url.scheme == SCM_MISSING || req->url.scheme == SCM_UNKNOWN) {
      // retry it as "http://"
      auto u = Strnew_m_charp("http://", req->url.file, NULL)->ptr;
      struct Url url;
      parseURL2(u, &url, req->current);
      req = newHttpRequest(url, req->form, req->referer, req->no_cache,
                           req->extra_header);
      http_response = openURL(req, of);
    }
  }

  if (http_response && http_response->stream_status == STREAM_MISSING) {
    ISclose(http_response->stream);
    return NULL;
  }

  /* openURL() succeeded */
  of = NULL;
  if (header_string) {
    header_string = NULL;
  }

  if (req->url.scheme == SCM_HTTP || req->url.scheme == SCM_HTTPS ||
      (((req->url.scheme == SCM_FTP && non_null(FTP_proxy))) && use_proxy &&
       !check_no_proxy(req->url.host))) {

    term_message(
        Sprintf("%s contacted. Waiting for reply...", req->url.host)->ptr);
    httpReadResponse(http_response);

    const char *p = httpGetHeader(http_response, "Location:");
    if (((http_response->http_status_code >= 301 &&
          http_response->http_status_code <= 303) ||
         http_response->http_status_code == 307) &&
        p && checkRedirection(&req->url)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      auto tpath = url_quote(p);
      struct Url url;
      // current = New(struct Url);
      // copyParsedURL(current, &pu);
      parseURL2(tpath, &url, &req->url);
      // form = NULL;
      req = newHttpRequest(url, nullptr, req->referer, req->no_cache,
                           req->extra_header);
      ISclose(http_response->stream);
      // t_buf->bufferprop |= BP_REDIRECTED;
      return sendHttpRequest(req, of, add_auth_cookie_flag, realm, uname, pwd);
    }

    if (add_auth_cookie_flag && realm && uname && pwd) {
      /* If authorization is required and passed */
      add_auth_user_passwd(&req->url, qstr_unquote(realm)->ptr, uname, pwd, 0);
      add_auth_cookie_flag = 0;
    }

    p = httpGetHeader(http_response, "WWW-Authenticate:");
    if (p && http_response->http_status_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, http_response, "WWW-Authenticate:") !=
              NULL &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        getAuthCookie(&hauth, "Authorization:", req, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          return http_response;
        }
        ISclose(http_response->stream);
        add_auth_cookie_flag = 1;
        return sendHttpRequest(req, of, add_auth_cookie_flag, realm, uname,
                               pwd);
      }
    }

    p = httpGetHeader(http_response, "Proxy-Authenticate:");
    if (p && http_response->http_status_code == 407) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, http_response, "Proxy-Authenticate:") &&
          (realm = get_auth_param(hauth.param, "realm")) != NULL) {
        auto auth_pu = schemeToProxy(req->url.scheme);
        getAuthCookie(&hauth, "Proxy-Authorization:", req, &uname, &pwd);
        if (uname == NULL) {
          /* abort */
          return http_response;
        }

        ISclose(http_response->stream);
        add_auth_cookie_flag = 1;
        add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        return sendHttpRequest(req, of, add_auth_cookie_flag, realm, uname,
                               pwd);
      }
    }

    /* XXX: RFC2617 3.2.3 Authentication-Info: ? */
    if (http_response->stream_status == STREAM_CONNECT) {
      return sendHttpRequest(req, http_response->stream, add_auth_cookie_flag,
                             realm, uname, pwd);
    }

    // f.modtime = mymktime(httpGetHeader(t_buf->http_response,
    // "Last-Modified:"));
  } else if (req->url.scheme == SCM_FTP) {
    // f.compression = check_compression(path, &f.guess_type);
    // if (f.compression != CMP_NOCOMPRESS) {
    //   auto t1 = uncompressed_file_type(pu.file, NULL);
    //   real_type = f.guess_type;
    //   if (t1)
    //     t = t1;
    //   else
    //     t = real_type;
    // } else
  } else if (req->url.scheme == SCM_DATA) {
    // t = f.guess_type;
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  // f.guess_type = t;

  return http_response;
}

struct Buffer *loadGeneralFile(int cols, const char *path, struct Url *current,
                               const char *referer, bool no_cahce,
                               struct FormList *form) {
  checkRedirection(NULL);

  struct Url url;
  parseURL2(path, &url, current);
  switch (url.scheme) {
  case SCM_LOCAL: {
    struct stat st;
    if (stat(url.real_file, &st) < 0) {
      return nullptr;
    }
    if (S_ISDIR(st.st_mode)) {
      if (UseExternalDirBuffer) {
        Str cmd = Sprintf("%s?dir=%s#current", DirBufferCommand, url.file);
        auto b = loadGeneralFile(cols, cmd->ptr, NULL, NO_REFERER, 0, NULL);
        if (b != NULL && b != NO_BUFFER) {
          copyParsedURL(&b->currentURL, &url);
          b->filename = b->currentURL.real_file;
        }
        return b;
      } else {
        auto page = loadLocalDir(url.real_file);
        auto t = "local:directory";
        // if (page && page->length > 0) {
        //   return get_document(cols, pu, stream, page, t, real_type, t_buf);
        // }
      }
    }
    return nullptr;
  }

  case SCM_FTPDIR: {
    auto page = loadFTPDir(&url, &charset);
    auto t = "ftp:directory";
    // if (page && page->length > 0) {
    //   return get_document(cols, url, stream, page, t, real_type, t_buf);
    // }
    // if (page) {
    //   auto tmp = tmpfname(TMPF_SRC, ".html");
    //   auto src = fopen(tmp->ptr, "w");
    //   if (src) {
    //     Strfputs(page, src);
    //     fclose(src);
    //   }
    //
    //   auto doc = loadHTML(cols, page->ptr, pu, nullptr, CHARSET_UNKONWN);
    //   if (!doc) {
    //     return nullptr;
    //   }
    //
    //   // copyParsedURL(&b->currentURL, &pu);
    //   // b->real_scheme = pu.scheme;
    //   // b->real_type = real_type;
    //   // if (src)
    //   //   b->sourcefile = tmp->ptr;
    //   auto b = newBuffer();
    //   b->document = doc;
    //   return b;
    // }

    assert(false);
    return nullptr;
  }

  case SCM_UNKNOWN:
    message_push(Sprintf("Unknown URI: %s", parsedURL2Str(&url)->ptr)->ptr);
    return NULL;

  default: {
    struct TextList *extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    bool add_auth_cookie_flag = false;
    auto req = newHttpRequest(url, form, referer, no_cahce, extra_header);
    auto res =
        sendHttpRequest(req, nullptr, add_auth_cookie_flag, realm, uname, pwd);
    Str content = StrISreadAll(res->stream);
    ISclose(res->stream);

    auto b = newBuffer();
    b->http_response = res;
    auto url = res->request->url;
    copyParsedURL(&b->currentURL, &res->request->url);
    b->filename = url.real_file ? url.real_file : url.file;
    if (content) {
      FILE *src = NULL;
      if (url.scheme != SCM_LOCAL) {
        auto tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src) {
          b->sourcefile = tmp->ptr;
          Strfputs(content, src);
          fclose(src);
        }
      }
    }

    // if (b != NULL) {
    //   if (b->buffername == NULL || b->buffername[0] == '\0') {
    //     b->buffername = httpGetHeader(b->http_response, "Subject:");
    //     if (b->buffername == NULL && b->filename != NULL)
    //       b->buffername = lastFileName(b->filename);
    //   }
    //   if (b->currentURL.scheme == SCM_UNKNOWN)
    //     b->currentURL.scheme = url.scheme;
    //   if (url.scheme == SCM_LOCAL && b->sourcefile == NULL)
    //     b->sourcefile = b->filename;
    //   if (is_html_type(t))
    //     b->type = "text/html";
    //   else
    //     b->type = "text/plain";
    // }
    //
    // if (b && b != NO_BUFFER) {
    //   b->real_scheme = url.scheme;
    //   b->real_type = real_type;
    //   if (url.label) {
    //     if (is_html_type(t)) {
    //       struct Anchor *a;
    //       a = searchURLLabel(b->document, url.label);
    //       if (a != NULL) {
    //         gotoLine(b->document, a->start.line);
    //         if (label_topline)
    //           b->document->topLine =
    //               lineSkip(&b->document->viewport, b->document->topLine,
    //                        b->document->lastLine,
    //                        b->document->currentLine->linenumber -
    //                            b->document->topLine->linenumber,
    //                        false);
    //         b->document->viewport.pos = a->start.pos;
    //         arrangeCursor(b->document);
    //       }
    //     } else { /* plain text */
    //       int l = atoi(url.label);
    //       gotoRealLine(b->document, l);
    //       b->document->viewport.pos = 0;
    //       arrangeCursor(b->document);
    //     }
    //   }
    // }
    if (header_string)
      header_string = NULL;
    if (b && b != NO_BUFFER)
      preFormUpdateBuffer(b);

    // t = guessContentType(req->url.file);
    // if (t == NULL)
    //   t = "text/plain";
    // real_type = t;
    // if (f.guess_type)
    //   t = f.guess_type;
    // else if (DefaultType) {
    //   // t = DefaultType;
    DefaultType = NULL;
    // }

    b->real_type = guessContentType(res->request->url.file);
    if (b->real_type == NULL) {
      b->real_type = "text/plain";
    }
    b->type = httpGetContentType(res);
    if (!b->type && res->request->url.file) {
      if (!((res->http_status_code >= 400 && res->http_status_code <= 407) ||
            (res->http_status_code >= 500 && res->http_status_code <= 505)))
        b->type = guessContentType(res->request->url.file);
    }
    if (!b->type) {
      b->type = "text/plain";
    }
    if (!b->real_type) {
      b->real_type = b->type;
    }

    b->document = get_document(cols, res, current, content, b->type);
    return b;
  }
  }
}
