#include "input/loader.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "buffer/message.h"
#include "file/tmpfile.h"
#include "input/content.h"
#include "input/ftp.h"
#include "input/http.h"
#include "input/http_auth.h"
#include "input/istream.h"
#include "input/localcgi.h"
#include "os.h"
#include "rc.h"
#include "siteconf.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>

bool UseExternalDirBuffer = true;
bool label_topline = false;
bool AutoUncompress = false;
const char *DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;

static int doFileMove(char *tmpf, char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

// static struct Document *get_document(int cols,
//                                      struct HttpResponse *http_response,
//                                      struct Url *currentURL, Str content,
//                                      const char *t) {
//
//   // f.current_content_length = 0;
//   // const char *p;
//   // if ((p = httpGetHeader(t_buf->http_response, "Content-Length:")) !=
//   NULL)
//   //   f.current_content_length = strtoclen(p);
//
//   // if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
//   //   uncompress_stream(&f, &pu.real_file);
//   // } else if (f.compression != CMP_NOCOMPRESS) {
//   //   if (is_text_type(t)) {
//   //     if (t_buf == NULL)
//   //       t_buf = newBuffer();
//   //     uncompress_stream(&f, &t_buf->sourcefile);
//   //     uncompressed_file_type(pu.file, &f.ext);
//   //   } else {
//   //     t = compress_application_type(f.compression);
//   //     f.compression = CMP_NOCOMPRESS;
//   //   }
//   // }
//
//   struct Document *document;
//   if (is_html_type(t)) {
//     document = renderHTML(cols, content->ptr, http_response->request->url,
//                           http_response->content_charset);
//   } else {
//     document = loadText(cols, content->ptr);
//   }
//
//   document->type = t;
//   return document;
// }

struct Content *loadGeneralFile(const char *path, struct Url *current,
                                const char *referer, bool no_cahce,
                                struct FormList *form) {
  clearRedirection();

  struct Url url = parseURL2(path, current);
  switch (url.scheme) {
  case SCM_LOCAL: {
    struct stat st;
    if (stat(url.real_file, &st) < 0) {
      return nullptr;
    }
    if (S_ISDIR(st.st_mode)) {
      if (UseExternalDirBuffer) {
        Str cmd = Sprintf("%s?dir=%s#current", DirBufferCommand, url.file);
        return loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0, NULL);
      } else {
        auto page = loadLocalDir(url.real_file);
        if (page && page->length > 0) {
          auto content = newContent(url, page->ptr, page->length,
                                    "local:directory", CHARSET_UTF8);
          return content;
        }
      }
    }
    return nullptr;
  }

  case SCM_FTPDIR: {
    auto page = loadFTPDir0(&url);
    if (!page) {
      return nullptr;
    }
    auto content =
        newContent(url, page->ptr, page->length, "ftp:directory", CHARSET_UTF8);

    auto tmp = tmpfname(TMPF_SRC, ".html");
    auto src = fopen(tmp->ptr, "w");
    if (src) {
      Strfputs(page, src);
      fclose(src);
      content->sourcefile = tmp->ptr;
    }
    return content;
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
    Str bytes = StrISreadAll(res->stream);
    ISclose(res->stream);

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
    // if (b && b != NO_BUFFER)
    //   preFormUpdateBuffer(b);

    auto type = httpGetContentType(res);
    if (!type && res->request->url.file) {
      if (!((res->http_status_code >= 400 && res->http_status_code <= 407) ||
            (res->http_status_code >= 500 && res->http_status_code <= 505)))
        type = guessContentType(res->request->url.file);
    }
    if (!type) {
      type = "text/plain";
    }

    auto content =
        newContent(url, bytes->ptr, bytes->length, type, res->content_charset);

    if (url.scheme != SCM_LOCAL) {
      auto tmp = tmpfname(TMPF_SRC, ".html");
      auto src = fopen(tmp->ptr, "w");
      if (src) {
        content->sourcefile = tmp->ptr;
        Strfputs(bytes, src);
        fclose(src);
      }
    }
    return content;
  }
  }
}
