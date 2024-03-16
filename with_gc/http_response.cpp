#include "http_response.h"
#include "istream.h"
#include "quote.h"
#include "matchattr.h"
#include "app.h"
#include "readallbytes.h"
// #include "content.h"
#include "cookie.h"
#include "Str.h"
#include "myctype.h"
#include "html/readbuffer.h"
#include "mimehead.h"
#include "etc.h"
#include <assert.h>
#include <sys/stat.h>

int FollowRedirection = 10;

HttpResponse::HttpResponse() {}

HttpResponse::~HttpResponse() {}

static bool same_url_p(const Url &pu1, const Url &pu2) {
  return (pu1.scheme == pu2.scheme && pu1.port == pu2.port &&
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

// static int is_text_type(std::string_view type) {
//   return type.empty() || type.starts_with("text/") ||
//          (type.starts_with("application/") &&
//           type.find("xhtml") != std::string::npos) ||
//          type.starts_with("message/");
// }

bool HttpResponse::checkRedirection(const Url &pu) {

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, pu.to_Str().c_str());
    App::instance().disp_err_message(tmp->ptr);
    return false;
  }

  for (auto &url : redirectins) {
    if (same_url_p(pu, url)) {
      // same url found !
      auto tmp = Sprintf("Redirection loop detected (%s)", pu.to_Str().c_str());
      App::instance().disp_err_message(tmp->ptr);
      return false;
    }
  }

  redirectins.push_back(pu);

  return true;
}

int HttpResponse::readHeader(const std::shared_ptr<input_stream> &stream,
                             const Url &url) {

  assert(this->document_header.empty());
  if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  while (true) {
    auto _tmp = stream->StrmyISgets();
    if (_tmp.empty()) {
      break;
    }

    auto tmp = cleanup_line(_tmp, HEADER_MODE);
    if (tmp[0] == '\n' || tmp[0] == '\r' || tmp[0] == '\0') {
      // there is no header
      break;
    }

    pushHeader(url, Strnew(tmp));
  }

  return http_response_code;
}

void HttpResponse::pushHeader(const Url &url, Str *lineBuf2) {
  if ((url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS) &&
      http_response_code == -1) {
    auto p = lineBuf2->ptr;
    while (*p && !IS_SPACE(*p))
      p++;
    while (*p && IS_SPACE(*p))
      p++;
    http_response_code = atoi(p);
    App::instance().message(lineBuf2->ptr);
  }
  if (!strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26)) {
    auto p = lineBuf2->ptr + 26;
    while (IS_SPACE(*p))
      p++;

    // if (!strncasecmp(p, "base64", 6))
    //   uf->encoding = ENC_BASE64;
    // else if (!strncasecmp(p, "quoted-printable", 16))
    //   uf->encoding = ENC_QUOTE;
    // else if (!strncasecmp(p, "uuencode", 8) ||
    //          !strncasecmp(p, "x-uuencode", 10))
    //   uf->encoding = ENC_UUENCODE;
    // else
    //   uf->encoding = ENC_7BIT;
  } else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17)) {
    // process_compression(lineBuf2, uf);
  } else if (use_cookie && accept_cookie &&
             check_cookie_accept_domain(url.host.c_str()) &&
             (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
              !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12))) {

    process_http_cookie(&url, lineBuf2);

  } else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
             url.scheme == SCM_LOCAL_CGI) {

    auto p = lineBuf2->ptr + 12;
    SKIP_BLANKS(p);
    Str *funcname = Strnew();
    while (*p && !IS_SPACE(*p))
      Strcat_char(funcname, *(p++));
    SKIP_BLANKS(p);

    std::string f = funcname->ptr;
    if (f.size()) {
      auto tmp = Strnew_charp(p);
      Strchop(tmp);
      // TODO:
      App::instance().task(0, f, tmp->ptr);
    }
  }
  auto view = remove_space(lineBuf2->ptr);
  document_header.push_back({view.begin(), view.end()});
}

const char *HttpResponse::getHeader(const char *field) const {
  if (!field) {
    return {};
  }
  auto len = strlen(field);
  for (auto &i : this->document_header) {
    if (!strncasecmp(i.c_str(), field, len)) {
      auto p = i.c_str() + len;
      while (*p && IS_SPACE(*p)) {
        ++p;
      }
      return p;
    }
  }
  return {};
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
  if (this->document_header.size()) {
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
  // if (f.scheme == SCM_LOCAL) {
  //   // no cache for local file
  //   return {};
  // }

  auto tmp = App::instance().tmpfname(TMPF_SRC, ".html");
  auto src = fopen(tmp.c_str(), "w");
  if (!src) {
    // fail to open file
    return {};
  }

  sourcefile = tmp;
  return src;
}

void HttpResponse::page_loaded(Url url,
                               const std::shared_ptr<input_stream> &stream) {
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
  //   if (url.scheme == SCM_LOCAL) {
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

  // if ((f->content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
  //   url.real_file = f->uncompress_stream();
  // } else if (f->compression != CMP_NOCOMPRESS) {
  //   if ((is_text_type(this->type))) {
  //     this->sourcefile = f->uncompress_stream();
  //     auto [_, ext] = uncompressed_file_type(url.file.c_str());
  //     f->ext = ext;
  //   } else {
  //     this->type = compress_application_type(f->compression)->mime_type;
  //     f->compression = CMP_NOCOMPRESS;
  //   }
  // }

  this->currentURL = url;
  this->filename = this->currentURL.real_file.size()
                       ? Strnew(this->currentURL.real_file)->ptr
                       : Strnew(this->currentURL.file)->ptr;

  // loadSomething(this, &b->layout);
  {
    // auto layout = &b->layout;
    // Buffer *buf = src;
    // if (this->is_html_type()) {
    //   loadHTMLstream(this, layout);
    // } else {
    //   loadBuffer(this, layout);
    // }

    // auto src = this->createSourceFile();

    // if (f->stream->IStype() != IST_ENCODED) {
    //   f->stream = newEncodedStream(f->stream, f->encoding);
    // }

    while (true) {
      char buf[4096];
      auto readSize = stream->ISread_n(buf, sizeof(buf));
      if (readSize == 0) {
        break;
      }
      // auto lineBuf2 = Strnew(_lineBuf2);
      auto before = this->raw.size();
      this->raw.resize(before + readSize);
      memcpy(this->raw.data() + before, buf, readSize);

      // if (src)
      //   Strfputs(lineBuf2, src);
    }
    // res->type = "text/html";
    // if (n_textarea)
    // formResetBuffer(layout, layout->formitem);
    // if (src) {
    //   fclose(src);
    // }

    // if (layout->title.empty() || layout->title[0] == '\0') {
    //   layout->title = this->getHeader("Subject:");
    //   if (layout->title.empty() && this->filename.size()) {
    //     layout->title = lastFileName(this->filename.c_str());
    //   }
    // }

    // if (this->currentURL.scheme == SCM_UNKNOWN) {
    //   this->currentURL.scheme = f->scheme;
    // }

    // if (f->scheme == SCM_LOCAL && this->sourcefile.empty()) {
    //   this->sourcefile = this->filename;
    // }

    // if (buf && buf != NO_BUFFER)
    // {
    //   // this->real_schema = this->f.scheme;
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

/* get last modified time */
std::string HttpResponse::last_modified() const {
  if (this->document_header.size()) {
    for (auto &i : document_header) {
      if (strncasecmp(i.c_str(), "Last-modified: ", 15) == 0) {
        return i.c_str() + 15;
      }
    }
    return "unknown";
  } else if (this->currentURL.scheme == SCM_LOCAL) {
    struct stat st;
    if (stat(this->currentURL.file.c_str(), &st) < 0)
      return "unknown";
    return ctime(&st.st_mtime);
  }
  return "unknown";
}

CompressionType HttpResponse::contentEncoding() const {

  if (auto p = this->getHeader("Content-Encoding:")) {
    if (std::string_view("gzip") == p) {
      return CMP_GZIP;
    }

    // TODO:
    assert(false);
  }

  return {};
}

std::string_view HttpResponse::getBody() {
  if (raw.empty()) {
    raw = ReadAllBytes(sourcefile);
  }

  auto encoding = contentEncoding();
  if (encoding == CMP_GZIP && !decoded) {
    if (is_gzip(raw)) {
      raw = decode_gzip(raw);
      decoded = true;
    } else {
      // TODO: ?
      decoded = true;
    }
  }

  return {(const char *)raw.data(), (const char *)raw.data() + raw.size()};
}
