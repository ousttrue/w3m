#include "loadproc.h"
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

static long long current_content_length;

static int is_text_type(const char *type) {
  return (type == NULL || type[0] == '\0' ||
          strncasecmp(type, "text/", 5) == 0 ||
          (strncasecmp(type, "application/", 12) == 0 &&
           strstr(type, "xhtml") != NULL) ||
          strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

int is_html_type(const char *type) {
  return (type && (strcasecmp(type, "text/html") == 0 ||
                   strcasecmp(type, "application/xhtml+xml") == 0));
}

/*
 * loadBuffer: read file and make new buffer
 */
Buffer *loadBuffer(UrlStream *uf, Buffer *newBuf) {
  FILE *src = NULL;
  char pre_lbuf = '\0';
  int nlines;
  Str *tmpf;
  Lineprop *propBuffer = NULL;
  // MySignalHandler prevtrap = NULL;

  if (newBuf == NULL)
    newBuf = new Buffer(INIT_BUFFER_WIDTH());

  // if (SETJMP(AbortLoading) != 0) {
  //   goto _end;
  // }
  // TRAP_ON;

  if (newBuf->info->sourcefile.empty() && uf->schema != SCM_LOCAL) {
    tmpf = tmpfname(TMPF_SRC, NULL);
    src = fopen(tmpf->ptr, "w");
    if (src)
      newBuf->info->sourcefile = tmpf->ptr;
  }

  nlines = 0;
  if (uf->stream->IStype() != IST_ENCODED) {
    uf->stream = newEncodedStream(uf->stream, uf->encoding);
  }

  // long long linelen = 0;
  // long long trbyte = 0;
  while (true) {
    auto _lineBuf2 = uf->stream->StrmyISgets(); //&& lineBuf2->length
    if (_lineBuf2.empty()) {
      break;
    }
    auto lineBuf2 = Strnew(_lineBuf2);
    if (src) {
      Strfputs(lineBuf2, src);
    }
    // linelen += lineBuf2->length;
    // showProgress(&linelen, &trbyte, current_content_length);
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
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    newBuf->addnewline(lineBuf2->ptr, propBuffer, lineBuf2->length,
                       FOLD_BUFFER_WIDTH(), nlines);
  }
  // _end:
  // TRAP_OFF;
  newBuf->topLine = newBuf->firstLine;
  newBuf->lastLine = newBuf->currentLine;
  newBuf->currentLine = newBuf->firstLine;
  // newBuf->trbyte = trbyte + linelen;
  if (src)
    fclose(src);

  return newBuf;
}

enum class LoadProc {
  Html,
  Buffer,
};

static Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(UrlStream *, Buffer *),
                          Buffer *defaultbuf) {
  FILE *f, *popen(const char *, const char *);
  Buffer *buf;

  if (cmd == NULL || *cmd == '\0')
    return NULL;
  f = popen(cmd, "r");
  if (f == NULL)
    return NULL;

  UrlStream uf(SCM_UNKNOWN, newFileStream(f, pclose));
  buf = loadproc(&uf, defaultbuf);
  return buf;
}

#define SHELLBUFFERNAME "*Shellout*"
Buffer *getshell(const char *cmd) {
  Buffer *buf;

  buf = loadcmdout((char *)cmd, loadBuffer, NULL);
  if (buf == NULL)
    return NULL;
  buf->info->filename = cmd;
  buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  return buf;
}

static Buffer *loadSomething(UrlStream *f, LoadProc loadproc, Buffer *src,
                             const Url &pu, const char *real_type) {
  Buffer *buf = nullptr;
  switch (loadproc) {
  case LoadProc::Html:
    buf = loadHTMLBuffer(f, src);
    buf->info->type = "text/html";
    break;
  case LoadProc::Buffer:
    buf = loadBuffer(f, src);
    buf->info->type = "text/plain";
    break;
  }

  if (buf->buffername == NULL || buf->buffername[0] == '\0') {
    buf->buffername = buf->info->getHeader("Subject:");
    if (buf->buffername == NULL && buf->info->filename != NULL)
      buf->buffername = lastFileName(buf->info->filename);
  }
  if (buf->info->currentURL.schema == SCM_UNKNOWN)
    buf->info->currentURL.schema = f->schema;
  if (f->schema == SCM_LOCAL && buf->info->sourcefile.empty())
    buf->info->sourcefile = buf->info->filename;

  if (buf && buf != NO_BUFFER) {
    buf->info->real_schema = f->schema;
    buf->info->real_type = real_type;
    if (pu.label.size()) {
      if (loadproc == LoadProc::Html) {
        Anchor *a;
        a = searchURLLabel(buf, pu.label.c_str());
        if (a != NULL) {
          gotoLine(buf, a->start.line);
          if (label_topline)
            buf->topLine = lineSkip(
                buf, buf->topLine,
                buf->currentLine->linenumber - buf->topLine->linenumber, false);
          buf->pos = a->start.pos;
          arrangeCursor(buf);
        }
      } else { /* plain text */
        int l = atoi(pu.label.c_str());
        gotoRealLine(buf, l);
        buf->pos = 0;
        arrangeCursor(buf);
      }
    }
  }
  return buf;
}

static int _MoveFile(const char *path1, const char *path2) {
  FILE *f2;
  int is_pipe;
  long long linelen = 0, trbyte = 0;
  char *buf = NULL;
  int count;

  auto f1 = openIS(path1);
  if (f1 == NULL)
    return -1;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = true;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = false;
    f2 = fopen(path2, "wb");
  }
  if (f2 == NULL) {
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
  const char *p, *q = NULL;
  pid_t pid;
  char *lock;
  struct stat st;
  long long size = 0;
  int is_pipe = false;

  if (fmInitialized) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      // q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
      //                   SaveHist);
      if (q == NULL || *q == '\0')
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
  return strtoll(s, NULL, 10);
#elif defined(HAVE_STRTOQ)
  return strtoq(s, NULL, 10);
#elif defined(HAVE_ATOLL)
  return atoll(s);
#elif defined(HAVE_ATOQ)
  return atoq(s);
#else
  return atoi(s);
#endif
}

static Buffer *page_loaded(const std::shared_ptr<HttpRequest> &hr,
                           Buffer *t_buf, UrlStream &f,
                           // Str *page,
                           const char *t, const char *real_type) {
  // if (page) {
  //   FILE *src;
  //   auto tmp = tmpfname(TMPF_SRC, ".html");
  //   src = fopen(tmp->ptr, "w");
  //   if (src) {
  //     Str *s;
  //     s = page;
  //     Strfputs(s, src);
  //     fclose(src);
  //   }
  //   if (do_download) {
  //     if (!src)
  //       return NULL;
  //     auto file = guess_filename(hr->url.file.c_str());
  //     doFileMove(tmp->ptr, file);
  //     return NO_BUFFER;
  //   }
  //   auto b = loadHTMLString(page);
  //   if (b) {
  //     b->info->currentURL = hr->url;
  //     b->info->real_schema = hr->url.schema;
  //     b->info->real_type = t;
  //     if (src) {
  //       b->info->sourcefile = tmp->ptr;
  //     }
  //   }
  //   return b;
  // }

  if (!real_type) {
    real_type = t;
  }

  current_content_length = 0;
  const char *p;
  if ((p = t_buf->info->getHeader("Content-Length:"))) {
    current_content_length = strtoclen(p);
  }
  if (do_download) {
    /* download only */
    const char *file;
    // TRAP_OFF;
    if (DecodeCTE && f.stream->IStype() != IST_ENCODED) {
      f.stream = newEncodedStream(f.stream, f.encoding);
    }
    if (hr->url.schema == SCM_LOCAL) {
      struct stat st;
      if (PreserveTimestamp && !stat(hr->url.real_file.c_str(), &st))
        f.modtime = st.st_mtime;
      file = guess_filename(hr->url.real_file.c_str());
    } else {
      file = t_buf->info->guess_save_name(hr->url.file.c_str());
    }
    f.doFileSave(file);
    return NO_BUFFER;
  }

  if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    hr->url.real_file = f.uncompress_stream();
  } else if (f.compression != CMP_NOCOMPRESS) {
    if ((is_text_type(t))) {
      if (t_buf == NULL)
        t_buf = new Buffer(INIT_BUFFER_WIDTH());
      t_buf->info->sourcefile = f.uncompress_stream();
      uncompressed_file_type(hr->url.file.c_str(), &f.ext);
    } else {
      t = compress_application_type(f.compression);
      f.compression = CMP_NOCOMPRESS;
    }
  }

  auto proc = LoadProc::Buffer;
  if (is_html_type(t)) {
    proc = LoadProc::Html;
  } else {
    proc = LoadProc::Buffer;
  }
  if (t_buf == NULL)
    t_buf = new Buffer(INIT_BUFFER_WIDTH());
  t_buf->info->currentURL = hr->url;
  t_buf->info->filename = hr->url.real_file.size()
                              ? Strnew(hr->url.real_file)->ptr
                              : Strnew(hr->url.file)->ptr;
  t_buf->ssl_certificate = (char *)f.ssl_certificate;

  auto b = loadSomething(&f, proc, t_buf, hr->url, real_type);
  // if (header_string)
  //   header_string = NULL;
  if (b && b != NO_BUFFER)
    preFormUpdateBuffer(b);
  // TRAP_OFF;
  return b;
}

/*
 * loadGeneralFile: load file to buffer
 */
Buffer *loadGeneralFile(const char *path, std::optional<Url> current,
                        const HttpOption &option, FormList *request) {
  UrlStream f(SCM_MISSING);

  // TRAP_OFF;
  auto hr = f.openURL(path, current, option, request);

  // Directory Open ?
  // if (f.stream == NULL) {
  //   switch (f.schema) {
  //   case SCM_LOCAL: {
  //     struct stat st;
  //     if (stat(pu.real_file.c_str(), &st) < 0)
  //       return NULL;
  //     if (S_ISDIR(st.st_mode)) {
  //       if (UseExternalDirBuffer) {
  //         Str *cmd =
  //             Sprintf("%s?dir=%s#current", DirBufferCommand,
  //             pu.file.c_str());
  //         auto b = loadGeneralFile(cmd->ptr, {}, {.referer = NO_REFERER});
  //         if (b != NULL && b != NO_BUFFER) {
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
  //   return NULL;
  // }

  if (hr && hr->status == HTST_MISSING) {
    // TRAP_OFF;
    return NULL;
  }

  /* openURL() succeeded */
  // if (SETJMP(AbortLoading) != 0) {
  //   /* transfer interrupted */
  //   TRAP_OFF;
  //   if (b)
  //     discardBuffer(b);
  //   return NULL;
  // }

  // b = NULL;
  // if (header_string){
  //   header_string = NULL;
  // }

  // TRAP_ON;
  const char *t = "text/plain";
  const char *real_type = NULL;
  auto t_buf = new Buffer(INIT_BUFFER_WIDTH());
  if (hr->url.schema == SCM_HTTP || hr->url.schema == SCM_HTTPS) {

    // if (fmInitialized) {
    //   term_cbreak();
    //   message(
    //       Sprintf("%s contacted. Waiting for reply...",
    //       hr->url.host.c_str())->ptr, 0, 0);
    //   refresh(term_io());
    // }
    auto http_response_code = t_buf->info->readHeader(&f, hr->url);
    const char *p;
    if (((http_response_code >= 301 && http_response_code <= 303) ||
         http_response_code == 307) &&
        (p = t_buf->info->getHeader("Location:")) != NULL &&
        t_buf->info->checkRedirection(hr->url)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      return loadGeneralFile(Strnew(url_quote(p))->ptr, hr->url, option, {});
    }

    t = t_buf->info->checkContentType();
    if (t == NULL && hr->url.file.size()) {
      if (!((http_response_code >= 400 && http_response_code <= 407) ||
            (http_response_code >= 500 && http_response_code <= 505)))
        t = guessContentType(hr->url.file.c_str());
    }
    if (t == NULL) {
      t = "text/plain";
    }
    hr->add_auth_cookie();
    if ((p = t_buf->info->getHeader("WWW-Authenticate:")) &&
        http_response_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, *t_buf->info, "WWW-Authenticate:") &&
          (hr->realm = get_auth_param(hauth.param, "realm"))) {
        getAuthCookie(&hauth, "Authorization:", hr->url, hr.get(), request,
                      &hr->uname, &hr->pwd);
        if (hr->uname == NULL) {
          /* abort */
          // TRAP_OFF;
          return page_loaded(hr, t_buf, f, t, {});
        }
        hr->add_auth_cookie_flag = true;
        // status = HTST_NORMAL;
        return loadGeneralFile(path, current, option, request);
      }
    }

    if (hr && hr->status == HTST_CONNECT) {
      return loadGeneralFile(path, current, option, request);
    }

    f.modtime = mymktime(t_buf->info->getHeader("Last-Modified:"));
  } else if (hr->url.schema == SCM_DATA) {
    t = f.guess_type;
  } else if (DefaultType) {
    t = DefaultType;
    DefaultType = NULL;
  } else {
    t = guessContentType(hr->url.file.c_str());
    if (!t) {
      t = "text/plain";
    }
    real_type = t;
    if (f.guess_type)
      t = f.guess_type;
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  f.guess_type = t;

  return page_loaded(hr, t_buf, f, t, real_type);
}

#define PIPEBUFFERNAME "*stream*"

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

static void _saveBuffer(Buffer *buf, Line *l, FILE *f, int cont) {

  auto is_html = is_html_type(buf->info->type);

  for (; l != NULL; l = l->next) {
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
  _saveBuffer(buf, buf->firstLine, f, cont);
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
  Str *tmp = NULL;
  const char *p;

  for (p = str; *p; p++) {
    if (is_shell_unsafe(*p)) {
      if (tmp == NULL)
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
