#include "loadproc.h"
#include "mimehead.h"
#include "url_quote.h"
#include "tabbuffer.h"
#include "quote.h"
#include "symbol.h"
#include "contentinfo.h"
#include "form.h"
#include "proto.h"
#include "downloadlist.h"
#include "istream.h"
#include "linein.h"
#include "mytime.h"
#include "authpass.h"
#include "httpauth.h"
#include "auth_digest.h"
#include "mimetypes.h"
#include "app.h"
#include "func.h"
#include "cookie.h"
#include "compression.h"
#include "display.h"
#include "screen.h"
#include "loaddirectory.h"
#include "url.h"
#include "alloc.h"
#include "rc.h"
#include "httprequest.h"
#include "display.h"
#include "matchattr.h"
#include "myctype.h"
#include "mailcap.h"
#include "etc.h"
#include "textlist.h"
#include "message.h"
#include "Str.h"
#include "istream.h"
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
#include <unistd.h>
#include <fcntl.h>
#define HAVE_ATOLL 1

const char *DefaultType = nullptr;
int FollowRedirection = 10;
bool DecodeCTE = false;

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG) {
  LONGJMP(AbortLoading, 1);
  SIGNAL_RETURN;
}

static long long current_content_length;

static int is_text_type(const char *type) {
  return (type == NULL || type[0] == '\0' ||
          strncasecmp(type, "text/", 5) == 0 ||
          (strncasecmp(type, "application/", 12) == 0 &&
           strstr(type, "xhtml") != NULL) ||
          strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int is_dump_text_type(const char *type) {
  struct MailcapEntry *mcap;
  return (type && (mcap = searchExtViewer(type)) &&
          (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int is_plain_text_type(const char *type) {
  return ((type && strcasecmp(type, "text/plain") == 0) ||
          (is_text_type(type) && !is_dump_text_type(type)));
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

  if (newBuf->sourcefile.empty() &&
      (uf->schema != SCM_LOCAL || newBuf->mailcap)) {
    tmpf = tmpfname(TMPF_SRC, NULL);
    src = fopen(tmpf->ptr, "w");
    if (src)
      newBuf->sourcefile = tmpf->ptr;
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

const char *checkHeader(Buffer *buf, const char *field) {
  int len;
  TextListItem *i;
  char *p;

  if (buf == NULL || field == NULL || buf->info->document_header == NULL)
    return NULL;
  len = strlen(field);
  for (i = buf->info->document_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, field, len)) {
      p = i->ptr + len;
      return remove_space(p);
    }
  }
  return NULL;
}

#define DEF_SAVE_FILE "index.html"
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
  if (buf && buf->info->document_header) {
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

static Buffer *loadSomething(UrlStream *f,
                             Buffer *(*loadproc)(UrlStream *, Buffer *),
                             Buffer *defaultbuf) {
  Buffer *buf;

  if ((buf = loadproc(f, defaultbuf)) == NULL)
    return NULL;

  if (buf->buffername == NULL || buf->buffername[0] == '\0') {
    buf->buffername = checkHeader(buf, "Subject:");
    if (buf->buffername == NULL && buf->info->filename != NULL)
      buf->buffername = lastFileName(buf->info->filename);
  }
  if (buf->info->currentURL.schema == SCM_UNKNOWN)
    buf->info->currentURL.schema = f->schema;
  if (f->schema == SCM_LOCAL && buf->sourcefile.empty())
    buf->sourcefile = buf->info->filename;
  if (loadproc == loadHTMLBuffer)
    buf->info->type = "text/html";
  else
    buf->info->type = "text/plain";
  return buf;
}

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
  uf.close();
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

Buffer *doExternal(UrlStream uf, const char *type, Buffer *defaultbuf) {
  Str *tmpf, *command;
  struct MailcapEntry *mcap;
  MailcapStat mc_stat;
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

  if (uf.stream->IStype() != IST_ENCODED) {
    uf.stream = newEncodedStream(uf.stream, uf.encoding);
  }
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
      setup_child(false, 0, uf.fileno());
      if (uf.save2tmp(tmpf->ptr) < 0)
        exit(1);
      uf.close();
      myExec(command->ptr);
    }
    return NO_BUFFER;
  } else {
    if (uf.save2tmp(tmpf->ptr) < 0) {
      return NULL;
    }
  }
  if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
    if (defaultbuf == NULL)
      defaultbuf = new Buffer(INIT_BUFFER_WIDTH());
    if (defaultbuf->sourcefile.size())
      src = Strnew(defaultbuf->sourcefile)->ptr;
    else
      src = tmpf->ptr;
    defaultbuf->sourcefile = {};
    defaultbuf->mailcap = mcap;
  }
  if (mcap->flags & MAILCAP_HTMLOUTPUT) {
    buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->info->type = "text/html";
      buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
      buf->sourcefile = (char *)src;
    }
  } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
    buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->info->type = "text/plain";
      buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
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
        buf->info->filename)
      buf->buffername = lastFileName(buf->info->filename);
    buf->edit = mcap->edit;
    buf->mailcap = mcap;
  }
  return buf;
}
#define DO_EXTERNAL ((Buffer * (*)(UrlStream *, Buffer *)) doExternal)

static int http_response_code;
void readHeader(UrlStream *uf, Buffer *newBuf, Url *pu) {
  char *p, *q;
  char c;
  Str *lineBuf2 = NULL;

  TextList *headerlist;
  FILE *src = NULL;
  Lineprop *propBuffer;

  headerlist = newBuf->info->document_header = newTextList();
  if (uf->schema == SCM_HTTP || uf->schema == SCM_HTTPS)
    http_response_code = -1;
  else
    http_response_code = 0;

  while (true) {
    auto _tmp = uf->StrmyUFgets();
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
      c = uf->getc();
      uf->undogetc();
      if (c == ' ' || c == '\t')
        /* header line is continued */
        continue;
      lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
      cleanup_line(lineBuf2, RAW_MODE);
      /* separated with line and stored */
      tmp = Strnew_size(lineBuf2->length);
      for (p = lineBuf2->ptr; *p; p = q) {
        for (q = p; *q && *q != '\r' && *q != '\n'; q++)
          ;
        lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer);
        Strcat(tmp, lineBuf2);
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
               check_cookie_accept_domain(pu->host.c_str()) &&
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
  if (src)
    fclose(src);
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

static bool same_url_p(const Url &pu1, const Url &pu2) {
  return (pu1.schema == pu2.schema && pu1.port == pu2.port &&
          (pu1.host.size() ? pu2.host.size() ? pu1.host == pu2.host : false
                           : true) &&
          (pu1.file.size() ? pu2.file.size() ? pu1.file == pu2.file : false
                           : true));
}

static bool checkRedirection(const Url *pu) {
  static std::vector<Url> redirectins;

  if (pu == nullptr) {
    // clear
    redirectins.clear();
    return true;
  }

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, pu->to_Str().c_str());
    disp_err_message(tmp->ptr, false);
    return false;
  }

  for (auto &url : redirectins) {
    if (same_url_p(*pu, url)) {
      // same url found !
      auto tmp =
          Sprintf("Redirection loop detected (%s)", pu->to_Str().c_str());
      disp_err_message(tmp->ptr, false);
      return false;
    }
  }

  redirectins.push_back(*pu);

  return true;
}
/*
 * loadGeneralFile: load file to buffer
 */

Buffer *loadGeneralFile(const char *path, std::optional<Url> current,
                        const HttpOption &option, FormList *request) {
  Url pu;
  Buffer *b = NULL;
  Buffer *(*proc)(UrlStream *, Buffer *) = loadBuffer;
  const char *tpath;
  const char *t = "text/plain";
  const char *p = NULL;
  const char *real_type = NULL;
  Buffer *t_buf = NULL;
  MySignalHandler prevtrap = NULL;
  TextList *extra_header = newTextList();
  Str *uname = NULL;
  Str *pwd = NULL;
  Str *realm = NULL;
  int add_auth_cookie_flag;
  Str *tmp;
  Str *page = NULL;
  HttpRequest hr;
  Url *auth_pu;

  tpath = path;
  prevtrap = NULL;
  add_auth_cookie_flag = 0;

  checkRedirection(NULL);

load_doc:
  pu = urlParse(tpath, current);

  UrlStream f(SCM_MISSING);
  // if (ouf) {
  //   uf = *ouf;
  // } else {
  // }

  TRAP_OFF;
  auto status =
      f.openURL(tpath, &pu, current, option, request, extra_header, &hr);

  if (f.stream == NULL) {
    switch (f.schema) {
    case SCM_LOCAL: {
      struct stat st;
      if (stat(pu.real_file.c_str(), &st) < 0)
        return NULL;
      if (S_ISDIR(st.st_mode)) {
        if (UseExternalDirBuffer) {
          Str *cmd =
              Sprintf("%s?dir=%s#current", DirBufferCommand, pu.file.c_str());
          b = loadGeneralFile(cmd->ptr, {}, {.referer = NO_REFERER});
          if (b != NULL && b != NO_BUFFER) {
            b->info->currentURL = pu;
            b->info->filename = Strnew(b->info->currentURL.real_file)->ptr;
          }
          return b;
        } else {
          page = loadLocalDir(pu.real_file.c_str());
          t = "local:directory";
        }
      }
    } break;

    case SCM_UNKNOWN:
      disp_err_message(Sprintf("Unknown URI: %s", pu.to_Str().c_str())->ptr,
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
    f.close();
    return NULL;
  }

  /* openURL() succeeded */
  if (SETJMP(AbortLoading) != 0) {
    /* transfer interrupted */
    TRAP_OFF;
    if (b)
      discardBuffer(b);
    f.close();
    return NULL;
  }

  b = NULL;
  if (header_string)
    header_string = NULL;
  TRAP_ON;
  if (pu.schema == SCM_HTTP || pu.schema == SCM_HTTPS) {

    if (fmInitialized) {
      term_cbreak();
      message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr, 0,
              0);
      refresh(term_io());
    }
    if (t_buf == NULL)
      t_buf = new Buffer(INIT_BUFFER_WIDTH());
    readHeader(&f, t_buf, &pu);
    if (((http_response_code >= 301 && http_response_code <= 303) ||
         http_response_code == 307) &&
        (p = checkHeader(t_buf, "Location:")) != NULL &&
        checkRedirection(&pu)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      tpath = Strnew(url_quote(p))->ptr;
      request = NULL;
      f.close();
      current = pu;
      t_buf = new Buffer(INIT_BUFFER_WIDTH());
      t_buf->bufferprop =
          static_cast<BufferFlags>(t_buf->bufferprop | BP_REDIRECTED);
      status = HTST_NORMAL;
      goto load_doc;
    }
    t = checkContentType(t_buf);
    if (t == NULL && pu.file.size()) {
      if (!((http_response_code >= 400 && http_response_code <= 407) ||
            (http_response_code >= 500 && http_response_code <= 505)))
        t = guessContentType(pu.file.c_str());
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
        f.close();
        add_auth_cookie_flag = 1;
        status = HTST_NORMAL;
        goto load_doc;
      }
    }

    if (status == HTST_CONNECT) {
      goto load_doc;
    }

    f.modtime = mymktime(checkHeader(t_buf, "Last-Modified:"));
  } else if (pu.schema == SCM_DATA) {
    t = f.guess_type;
  } else if (DefaultType) {
    t = DefaultType;
    DefaultType = NULL;
  } else {
    t = guessContentType(pu.file.c_str());
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
      auto file = guess_filename(pu.file.c_str());
      doFileMove(tmp->ptr, file);
      return NO_BUFFER;
    }
    b = loadHTMLString(page);
    if (b) {
      b->info->currentURL = pu;
      b->real_schema = pu.schema;
      b->info->real_type = t;
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
    if (DecodeCTE && f.stream->IStype() != IST_ENCODED) {
      f.stream = newEncodedStream(f.stream, f.encoding);
    }
    if (pu.schema == SCM_LOCAL) {
      struct stat st;
      if (PreserveTimestamp && !stat(pu.real_file.c_str(), &st))
        f.modtime = st.st_mtime;
      file = guess_save_name(NULL, pu.real_file.c_str());
    } else
      file = guess_save_name(t_buf, pu.file.c_str());
    f.doFileSave(file);
    f.close();
    return NO_BUFFER;
  }

  if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
    pu.real_file = f.uncompress_stream();
  } else if (f.compression != CMP_NOCOMPRESS) {
    if ((is_text_type(t) || searchExtViewer(t))) {
      if (t_buf == NULL)
        t_buf = new Buffer(INIT_BUFFER_WIDTH());
      t_buf->sourcefile = f.uncompress_stream();
      uncompressed_file_type(pu.file.c_str(), &f.ext);
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
    if (!do_download && searchExtViewer(t) != NULL) {
      proc = DO_EXTERNAL;
    } else {
      TRAP_OFF;
      if (pu.schema == SCM_LOCAL) {
        f.close();
        _doFileCopy(pu.real_file.c_str(),
                    guess_save_name(NULL, pu.real_file.c_str()), true);
      } else {
        if (DecodeCTE && f.stream->IStype() != IST_ENCODED) {
          f.stream = newEncodedStream(f.stream, f.encoding);
        }
        f.doFileSave(guess_save_name(t_buf, pu.file.c_str()));
        f.close();
      }
      return NO_BUFFER;
    }
  }
  if (t_buf == NULL)
    t_buf = new Buffer(INIT_BUFFER_WIDTH());
  t_buf->info->currentURL = pu;
  t_buf->info->filename =
      pu.real_file.size() ? Strnew(pu.real_file)->ptr : Strnew(pu.file)->ptr;
  t_buf->ssl_certificate = (char *)f.ssl_certificate;
  if (proc == DO_EXTERNAL) {
    b = doExternal(f, t, t_buf);
  } else {
    b = loadSomething(&f, proc, t_buf);
  }
  f.close();
  if (b && b != NO_BUFFER) {
    b->real_schema = f.schema;
    b->info->real_type = real_type;
    if (pu.label.size()) {
      if (proc == loadHTMLBuffer) {
        Anchor *a;
        a = searchURLLabel(b, pu.label.c_str());
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
        int l = atoi(pu.label.c_str());
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

void cmd_loadBuffer(Buffer *buf, int prop, int linkid) {
  if (buf == nullptr) {
    disp_err_message("Can't load string", false);
  } else if (buf != NO_BUFFER) {
    buf->bufferprop = (BufferFlags)(buf->bufferprop | BP_INTERNAL | prop);
    if (!(buf->bufferprop & BP_NO_URL))
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
