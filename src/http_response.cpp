#include "http_response.h"
#include "quote.h"
#include "matchattr.h"
#include "app.h"
#include "screen.h"
#include "cookie.h"
#include "Str.h"
#include "message.h"
#include "textlist.h"
#include "w3m.h"
#include "myctype.h"
#include "url_stream.h"
#include "html/readbuffer.h"
#include "mimehead.h"
#include "terms.h"
#include "func.h"
#include "etc.h"

#include <assert.h>
#include <sys/stat.h>

int FollowRedirection = 10;
bool DecodeCTE = false;

HttpResponse::HttpResponse() : f(SCM_MISSING) {}

HttpResponse::~HttpResponse() {}

static bool same_url_p(const Url &pu1, const Url &pu2) {
  return (pu1.schema == pu2.schema && pu1.port == pu2.port &&
          (pu1.host.size() ? pu2.host.size() ? pu1.host == pu2.host : false
                           : true) &&
          (pu1.file.size() ? pu2.file.size() ? pu1.file == pu2.file : false
                           : true));
}

static long long strtoclen(const char *s) {
#ifdef HAVE_STRTOLL
  return strtoll(s, nullptr, 10);
#elif defined(HAVE_STRTOQ)
  return strtoq(s, nullptr, 10);
#elif defined(HAVE_ATOLL)
  return atoll(s);
#elif defined(HAVE_ATOQ)
  return atoq(s);
#else
  return atoi(s);
#endif
}

static int is_text_type(std::string_view type) {
  return type.empty() || type.starts_with("text/") ||
         (type.starts_with("application/") &&
          type.find("xhtml") != std::string::npos) ||
         type.starts_with("message/");
}

bool HttpResponse::checkRedirection(const Url &pu) {

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, pu.to_Str().c_str());
    disp_err_message(tmp->ptr);
    return false;
  }

  for (auto &url : redirectins) {
    if (same_url_p(pu, url)) {
      // same url found !
      auto tmp = Sprintf("Redirection loop detected (%s)", pu.to_Str().c_str());
      disp_err_message(tmp->ptr);
      return false;
    }
  }

  redirectins.push_back(pu);

  return true;
}

int HttpResponse::readHeader(UrlStream *uf, const Url &url) {

  this->document_header = newTextList();
  if (url.schema == SCM_HTTP || url.schema == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  FILE *src = NULL;
  Str *lineBuf2 = NULL;
  while (true) {
    auto _tmp = uf->stream->StrmyISgets();
    if (_tmp.empty()) {
      break;
    }
    auto tmp = Strnew(_tmp);
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

      auto c = uf->stream ? uf->stream->ISgetc() : '\0';

      uf->stream->ISundogetc();
      if (c == ' ' || c == '\t')
        /* header line is continued */
        continue;
      lineBuf2 = decodeMIME0(lineBuf2);
      cleanup_line(lineBuf2, RAW_MODE);
      /* separated with line and stored */
      tmp = Strnew_size(lineBuf2->length);
      char *p, *q;
      for (p = lineBuf2->ptr; *p; p = q) {
        for (q = p; *q && *q != '\r' && *q != '\n'; q++)
          ;
        Lineprop *propBuffer;
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
        Strcat(tmp, lineBuf2);
        for (; *q && (*q == '\r' || *q == '\n'); q++)
          ;
      }
      lineBuf2 = tmp;
    }

    if ((url.schema == SCM_HTTP || url.schema == SCM_HTTPS) &&
        http_response_code == -1) {
      auto p = lineBuf2->ptr;
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
      auto p = lineBuf2->ptr + 26;
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
    } else if (use_cookie && accept_cookie &&
               check_cookie_accept_domain(url.host.c_str()) &&
               (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {

      process_http_cookie(&url, lineBuf2);

    } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
               url.schema == SCM_LOCAL_CGI) {

      auto p = lineBuf2->ptr + 12;
      SKIP_BLANKS(p);
      Str *funcname = Strnew();
      while (*p && !IS_SPACE(*p))
        Strcat_char(funcname, *(p++));
      SKIP_BLANKS(p);

      auto f = getFuncList(funcname->ptr);
      if (f >= 0) {
        tmp = Strnew_charp(p);
        Strchop(tmp);
        // TODO:
        App::instance().task(0, f, tmp->ptr);
      }
    }
    pushText(document_header, lineBuf2->ptr);
    Strfree(lineBuf2);
    lineBuf2 = NULL;
  }
  if (src) {
    fclose(src);
  }

  return http_response_code;
}

const char *HttpResponse::getHeader(const char *field) const {
  if (field == NULL || this->document_header == NULL) {
    return NULL;
  }

  auto len = strlen(field);
  for (auto i = this->document_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, field, len)) {
      auto p = i->ptr + len;
      return remove_space(p);
    }
  }
  return NULL;
}

const char *HttpResponse::checkContentType() const {
  auto p = this->getHeader("Content-Type:");
  if (p == NULL) {
    return NULL;
  }

  auto r = Strnew();
  while (*p && *p != ';' && !IS_SPACE(*p))
    Strcat_char(r, *p++);
  return r->ptr;
}

const char *mybasename(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  while (s <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s;
  return allocStr(p, -1);
}

#define DEF_SAVE_FILE "index.html"
const char *guess_filename(const char *file) {
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

const char *HttpResponse::guess_save_name(const char *path) const {
  if (this->document_header) {
    Str *name = NULL;
    const char *p, *q;
    if ((p = this->getHeader("Content-Disposition:")) != NULL &&
        (q = strcasestr(p, "filename")) != NULL &&
        (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
        matchattr(q, "filename", 8, &name))
      path = name->ptr;
    else if ((p = this->getHeader("Content-Type:")) != NULL &&
             (q = strcasestr(p, "name")) != NULL &&
             (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
             matchattr(q, "name", 4, &name))
      path = name->ptr;
  }
  return guess_filename(path);
}

FILE *HttpResponse::createSourceFile() {
  if (sourcefile.size()) {
    // already created
    return {};
  }
  if (f.schema == SCM_LOCAL) {
    // no cache for local file
    return {};
  }

  auto tmp = App::instance().tmpfname(TMPF_SRC, ".html");
  auto src = fopen(tmp.c_str(), "w");
  if (!src) {
    // fail to open file
    return {};
  }

  sourcefile = tmp;
  return src;
}

void HttpResponse::page_loaded(Url url) {
  const char *p;
  if ((p = this->getHeader("Content-Length:"))) {
    this->current_content_length = strtoclen(p);
  }

  // if (do_download) {
  //   /* download only */
  //   const char *file;
  //   if (DecodeCTE && this->f.stream->IStype() != IST_ENCODED) {
  //     this->f.stream = newEncodedStream(this->f.stream, this->f.encoding);
  //   }
  //   if (url.schema == SCM_LOCAL) {
  //     struct stat st;
  //     if (PreserveTimestamp && !stat(url.real_file.c_str(), &st))
  //       this->f.modtime = st.st_mtime;
  //     file = guess_filename(url.real_file.c_str());
  //   } else {
  //     file = this->guess_save_name(url.file.c_str());
  //   }
  //   this->f.doFileSave(file);
  //   return NO_BUFFER;
  // }

  if ((this->f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    url.real_file = this->f.uncompress_stream();
  } else if (this->f.compression != CMP_NOCOMPRESS) {
    if ((is_text_type(this->type))) {
      this->sourcefile = this->f.uncompress_stream();
      auto [_, ext] = uncompressed_file_type(url.file.c_str());
      this->f.ext = ext;
    } else {
      this->type = compress_application_type(this->f.compression)->mime_type;
      this->f.compression = CMP_NOCOMPRESS;
    }
  }

  this->currentURL = url;
  this->filename = this->currentURL.real_file.size()
                       ? Strnew(this->currentURL.real_file)->ptr
                       : Strnew(this->currentURL.file)->ptr;
  this->ssl_certificate = this->f.ssl_certificate;

  // loadSomething(this, &b->layout);
  {
    // auto layout = &b->layout;
    // Buffer *buf = src;
    // if (this->is_html_type()) {
    //   loadHTMLstream(this, layout);
    // } else {
    //   loadBuffer(this, layout);
    // }

    auto src = this->createSourceFile();

    if (this->f.stream->IStype() != IST_ENCODED) {
      this->f.stream = newEncodedStream(this->f.stream, this->f.encoding);
    }

    while (true) {
      auto _lineBuf2 = this->f.stream->StrmyISgets();
      if (_lineBuf2.empty()) {
        break;
      }
      auto lineBuf2 = Strnew(_lineBuf2);

      if (src)
        Strfputs(lineBuf2, src);
    }
    // res->type = "text/html";
    // if (n_textarea)
    // formResetBuffer(layout, layout->formitem);
    if (src) {
      fclose(src);
    }

    // if (layout->title.empty() || layout->title[0] == '\0') {
    //   layout->title = this->getHeader("Subject:");
    //   if (layout->title.empty() && this->filename.size()) {
    //     layout->title = lastFileName(this->filename.c_str());
    //   }
    // }

    if (this->currentURL.schema == SCM_UNKNOWN) {
      this->currentURL.schema = this->f.schema;
    }

    if (this->f.schema == SCM_LOCAL && this->sourcefile.empty()) {
      this->sourcefile = this->filename;
    }

    // if (buf && buf != NO_BUFFER)
    // {
    //   // this->real_schema = this->f.schema;
    //   // this->real_type = real_type;
    //   if (this->currentURL.label.size()) {
    //     if (this->is_html_type()) {
    //       auto a = layout->searchURLLabel(this->currentURL.label.c_str());
    //       if (a != nullptr) {
    //         layout->gotoLine(a->start.line);
    //         if (label_topline)
    //           layout->topLine = layout->lineSkip(
    //               layout->topLine,
    //               layout->currentLine->linenumber -
    //               layout->topLine->linenumber, false);
    //         layout->pos = a->start.pos;
    //         layout->arrangeCursor();
    //       }
    //     } else { /* plain text */
    //       int l = atoi(this->currentURL.label.c_str());
    //       layout->gotoRealLine(l);
    //       layout->pos = 0;
    //       layout->arrangeCursor();
    //     }
    //   }
    // }
  }

  // auto b = new Buffer(INIT_BUFFER_WIDTH());
  // b->info = shared_from_this();
  // preFormUpdateBuffer(b);
  // return b;
}
