#include "http_session.h"
#include "url_quote.h"
#include "tabbuffer.h"
#include "quote.h"
#include "symbol.h"
#include "http_response.h"
#include "form.h"
#include "downloadlist.h"
#include "linein.h"
#include "mytime.h"
#include "http_auth.h"
#include "mimetypes.h"
#include "app.h"
#include "display.h"
#include "alloc.h"
#include "myctype.h"
#include "etc.h"
#include "message.h"
#include "Str.h"
#include "w3m.h"
#include "tmpfile.h"
#include "url_stream.h"
#include "readbuffer.h"
#include "line.h"
#include "terms.h"
#include "signal_util.h"
#include "buffer.h"
#include <utime.h>
#include <sys/stat.h>
#include <string.h>
#define HAVE_ATOLL 1

const char *DefaultType = nullptr;
bool DecodeCTE = false;

static int is_text_type(std::string_view type) {
  return type.empty() || type.starts_with("text/") ||
         (type.starts_with("application/") &&
          type.find("xhtml") != std::string::npos) ||
         type.starts_with("message/");
}

void loadBuffer(const std::shared_ptr<HttpResponse> &res, LineLayout *layout) {
  FILE *src = nullptr;
  if (res->sourcefile.empty() && res->f.schema != SCM_LOCAL) {
    auto tmpf = tmpfname(TMPF_SRC, nullptr);
    src = fopen(tmpf->ptr, "w");
    if (src) {
      res->sourcefile = tmpf->ptr;
    }
  }

  auto nlines = 0;
  if (res->f.stream->IStype() != IST_ENCODED) {
    res->f.stream = newEncodedStream(res->f.stream, res->f.encoding);
  }

  char pre_lbuf = '\0';
  while (true) {
    auto _lineBuf2 = res->f.stream->StrmyISgets(); //&& lineBuf2->length
    if (_lineBuf2.empty()) {
      break;
    }
    auto lineBuf2 = Strnew(_lineBuf2);
    if (src) {
      Strfputs(lineBuf2, src);
    }
    cleanup_line(lineBuf2, PAGER_MODE);
    if (squeezeBlankLine) {
      if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        continue;
      }
      pre_lbuf = lineBuf2->ptr[0];
    }
    ++nlines;
    Strchop(lineBuf2);

    Lineprop *propBuffer = nullptr;
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    layout->addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length,
                       FOLD_BUFFER_WIDTH(), nlines);
  }
  layout->topLine = layout->firstLine;
  layout->lastLine = layout->currentLine;
  layout->currentLine = layout->firstLine;
  if (src) {
    fclose(src);
  }
  res->type = "text/plain";
}

Buffer *loadHTMLString(Str *page) {
  auto newBuf = new Buffer(INIT_BUFFER_WIDTH());
  newBuf->info->f = UrlStream(SCM_LOCAL, newStrStream(page->ptr));

  loadHTMLstream(newBuf->info, &newBuf->layout, true);

  return newBuf;
}

#define SHELLBUFFERNAME "*Shellout*"
Buffer *getshell(const char *cmd) {
  if (cmd == nullptr || *cmd == '\0') {
    return nullptr;
  }

  auto f = popen(cmd, "r");
  if (f == nullptr) {
    return nullptr;
  }

  auto buf = new Buffer(INIT_BUFFER_WIDTH());
  buf->info->f = UrlStream(SCM_UNKNOWN, newFileStream(f, pclose));
  loadBuffer(buf->info, &buf->layout);
  buf->info->filename = cmd;
  buf->layout.title = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

static void loadSomething(const std::shared_ptr<HttpResponse> &res,
                          LineLayout *layout) {
  // Buffer *buf = src;
  if (res->is_html_type()) {
    loadHTMLstream(res, layout);
  } else {
    loadBuffer(res, layout);
  }

  if (layout->title.empty() || layout->title[0] == '\0') {
    layout->title = res->getHeader("Subject:");
    if (layout->title.empty() && res->filename.size()) {
      layout->title = lastFileName(res->filename.c_str());
    }
  }
  if (res->currentURL.schema == SCM_UNKNOWN)
    res->currentURL.schema = res->f.schema;
  if (res->f.schema == SCM_LOCAL && res->sourcefile.empty())
    res->sourcefile = res->filename;

  // if (buf && buf != NO_BUFFER)
  {
    // res->real_schema = res->f.schema;
    // res->real_type = real_type;
    if (res->currentURL.label.size()) {
      if (res->is_html_type()) {
        auto a = layout->searchURLLabel(res->currentURL.label.c_str());
        if (a != nullptr) {
          layout->gotoLine(a->start.line);
          if (label_topline)
            layout->topLine = layout->lineSkip(layout->topLine,
                                               layout->currentLine->linenumber -
                                                   layout->topLine->linenumber,
                                               false);
          layout->pos = a->start.pos;
          layout->arrangeCursor();
        }
      } else { /* plain text */
        int l = atoi(res->currentURL.label.c_str());
        layout->gotoRealLine(l);
        layout->pos = 0;
        layout->arrangeCursor();
      }
    }
  }
}

static int _MoveFile(const char *path1, const char *path2) {
  FILE *f2;
  int is_pipe;
  long long linelen = 0, trbyte = 0;
  char *buf = nullptr;
  int count;

  auto f1 = openIS(path1);
  if (f1 == nullptr)
    return -1;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = true;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = false;
    f2 = fopen(path2, "wb");
  }
  if (f2 == nullptr) {
    f1->close();
    return -1;
  }

  auto current_content_length = 0;
  buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
  while ((count = f1->ISread_n(buf, SAVE_BUF_SIZE)) > 0) {
    fwrite(buf, 1, count, f2);
    linelen += count;
    showProgress(&linelen, &trbyte, current_content_length);
  }

  xfree(buf);
  f1->close();
  if (is_pipe)
    pclose(f2);
  else
    fclose(f2);
  return 0;
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

int _doFileCopy(const char *tmpf, const char *defstr, int download) {
  Str *msg;
  Str *filen;
  const char *p, *q = nullptr;
  pid_t pid;
  char *lock;
  struct stat st;
  long long size = 0;
  int is_pipe = false;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == nullptr || *p == '\0') {
      // q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
      //                   SaveHist);
      if (q == nullptr || *q == '\0')
        return false;
      p = q;
    }
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = true;
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
      disp_err_message(msg->ptr, false);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        msg = Sprintf("Can't save to %s", p);
        disp_err_message(msg->ptr, false);
      }
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
    symlink(p, lock);
    flush_tty();
    pid = fork();
    if (!pid) {
      setup_child(false, 0, -1);
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
    if (q == nullptr || *q == '\0') {
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
      is_pipe = true;
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

static Buffer *page_loaded(const std::shared_ptr<HttpRequest> &hr,
                           const std::shared_ptr<HttpResponse> &res) {
  const char *p;
  if ((p = res->getHeader("Content-Length:"))) {
    res->current_content_length = strtoclen(p);
  }
  if (do_download) {
    /* download only */
    const char *file;
    if (DecodeCTE && res->f.stream->IStype() != IST_ENCODED) {
      res->f.stream = newEncodedStream(res->f.stream, res->f.encoding);
    }
    if (hr->url.schema == SCM_LOCAL) {
      struct stat st;
      if (PreserveTimestamp && !stat(hr->url.real_file.c_str(), &st))
        res->f.modtime = st.st_mtime;
      file = guess_filename(hr->url.real_file.c_str());
    } else {
      file = res->guess_save_name(hr->url.file.c_str());
    }
    res->f.doFileSave(file);
    return NO_BUFFER;
  }

  if ((res->f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    hr->url.real_file = res->f.uncompress_stream();
  } else if (res->f.compression != CMP_NOCOMPRESS) {
    if ((is_text_type(res->type))) {
      res->sourcefile = res->f.uncompress_stream();
      uncompressed_file_type(hr->url.file.c_str(), &res->f.ext);
    } else {
      res->type = compress_application_type(res->f.compression);
      res->f.compression = CMP_NOCOMPRESS;
    }
  }

  res->currentURL = hr->url;
  res->filename = hr->url.real_file.size() ? Strnew(hr->url.real_file)->ptr
                                           : Strnew(hr->url.file)->ptr;
  res->ssl_certificate = res->f.ssl_certificate;

  auto b = new Buffer(INIT_BUFFER_WIDTH());
  b->info = res;
  loadSomething(res, &b->layout);
  preFormUpdateBuffer(b);
  return b;
}

/*
 * loadGeneralFile: load file to buffer
 */
Buffer *loadGeneralFile(const char *path, std::optional<Url> current,
                        const HttpOption &option, FormList *request) {
  auto res = std::make_shared<HttpResponse>();

  // TRAP_OFF;
  auto hr = res->f.openURL(path, current, option, request);

  // Directory Open ?
  // if (f.stream == nullptr) {
  //   switch (f.schema) {
  //   case SCM_LOCAL: {
  //     struct stat st;
  //     if (stat(pu.real_file.c_str(), &st) < 0)
  //       return nullptr;
  //     if (S_ISDIR(st.st_mode)) {
  //       if (UseExternalDirBuffer) {
  //         Str *cmd =
  //             Sprintf("%s?dir=%s#current", DirBufferCommand,
  //             pu.file.c_str());
  //         auto b = loadGeneralFile(cmd->ptr, {}, {.referer = NO_REFERER});
  //         if (b != nullptr && b != NO_BUFFER) {
  //           b->info->currentURL = pu;
  //           b->info->filename = Strnew(b->info->currentURL.real_file)->ptr;
  //         }
  //         return b;
  //       } else {
  //         page = loadLocalDir(pu.real_file.c_str());
  //         t = "local:directory";
  //       }
  //     }
  //   } break;
  //
  //   case SCM_UNKNOWN:
  //     disp_err_message(Sprintf("Unknown URI: %s", pu.to_Str().c_str())->ptr,
  //                      false);
  //     break;
  //
  //   default:
  //     break;
  //   }
  //   if (page && page->length > 0)
  //     return page_loaded();
  //   return nullptr;
  // }

  if (hr && hr->status == HTST_MISSING) {
    return nullptr;
  }

  if (hr->url.schema == SCM_HTTP || hr->url.schema == SCM_HTTPS) {

    // if (fmInitialized) {
    //   term_cbreak();
    //   message(
    //       Sprintf("%s contacted. Waiting for reply...",
    //       hr->url.host.c_str())->ptr, 0, 0);
    //   refresh(term_io());
    // }
    auto http_response_code = res->readHeader(&res->f, hr->url);
    const char *p;
    if (((http_response_code >= 301 && http_response_code <= 303) ||
         http_response_code == 307) &&
        (p = res->getHeader("Location:")) != nullptr &&
        res->checkRedirection(hr->url)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      return loadGeneralFile(Strnew(url_quote(p))->ptr, hr->url, option, {});
    }

    res->type = res->checkContentType();
    if (res->type.empty() && hr->url.file.size()) {
      if (!((http_response_code >= 400 && http_response_code <= 407) ||
            (http_response_code >= 500 && http_response_code <= 505)))
        res->type = guessContentType(hr->url.file.c_str());
    }
    if (res->type.empty()) {
      res->type = "text/plain";
    }
    hr->add_auth_cookie();
    if ((p = res->getHeader("WWW-Authenticate:")) &&
        http_response_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, *res, "WWW-Authenticate:") &&
          (hr->realm = get_auth_param(hauth.param, "realm"))) {
        getAuthCookie(&hauth, "Authorization:", hr->url, hr.get(), request,
                      &hr->uname, &hr->pwd);
        if (hr->uname == nullptr) {
          /* abort */
          // TRAP_OFF;
          return page_loaded(hr, res);
        }
        hr->add_auth_cookie_flag = true;
        return loadGeneralFile(path, current, option, request);
      }
    }

    if (hr && hr->status == HTST_CONNECT) {
      return loadGeneralFile(path, current, option, request);
    }

    res->f.modtime = mymktime(res->getHeader("Last-Modified:"));
  } else if (hr->url.schema == SCM_DATA) {
    res->type = res->f.guess_type;
  } else if (DefaultType) {
    res->type = DefaultType;
    DefaultType = nullptr;
  } else {
    res->type = guessContentType(hr->url.file.c_str());
    if (res->type.empty()) {
      res->type = "text/plain";
    }
    if (res->f.guess_type) {
      res->type = res->f.guess_type;
    }
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  res->f.guess_type = Strnew(res->type)->ptr;

  return page_loaded(hr, res);
}

#define PIPEBUFFERNAME "*stream*"

int setModtime(const char *path, time_t modtime) {
  struct utimbuf t;
  struct stat st;

  if (stat(path, &st) == 0)
    t.actime = st.st_atime;
  else
    t.actime = time(nullptr);
  t.modtime = modtime;
  return utime(path, &t);
}

static void _saveBuffer(Buffer *buf, Line *l, FILE *f, int cont) {

  auto is_html = buf->info->is_html_type();

  for (; l != nullptr; l = l->next) {
    Str *tmp;
    if (is_html)
      tmp = conv_symbol(l);
    else
      tmp = Strnew_charp_n(l->lineBuf.data(), l->len);
    Strfputs(tmp, f);
    if (Strlastchar(tmp) != '\n' && !(cont && l->next && l->next->bpos))
      putc('\n', f);
  }
}

void saveBuffer(Buffer *buf, FILE *f, int cont) {
  _saveBuffer(buf, buf->layout.firstLine, f, cont);
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

const char *shell_quote(const char *str) {
  Str *tmp = nullptr;
  const char *p;

  for (p = str; *p; p++) {
    if (is_shell_unsafe(*p)) {
      if (tmp == nullptr)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '\\');
      Strcat_char(tmp, *p);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

void cmd_loadBuffer(Buffer *buf, int linkid) {
  if (buf == nullptr) {
    disp_err_message("Can't load string", false);
  } else if (buf != NO_BUFFER) {
    buf->info->currentURL = Currentbuf->info->currentURL;
    if (linkid != LB_NOLINK) {
      buf->linkBuffer[linkid] = Currentbuf;
      Currentbuf->linkBuffer[linkid] = buf;
    }
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void cmd_loadfile(const char *fn) {

  auto buf =
      loadGeneralFile(file_to_url((char *)fn), {}, {.referer = NO_REFERER});
  if (buf == nullptr) {
    char *emsg = Sprintf("%s not found", fn)->ptr;
    disp_err_message(emsg, false);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}
