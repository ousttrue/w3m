#include <unistd.h>
#include "alloc.h"
#include "url_quote.h"
#include "tabbuffer.h"
#include "quote.h"
#include "http_response.h"
#include "http_session.h"
#include "bufferpos.h"
#include "app.h"
#include "etc.h"
#include "mimetypes.h"
#include "url_schema.h"
#include "url_stream.h"
#include "ssl_util.h"
#include "message.h"
#include "screen.h"
#include "tmpfile.h"
#include "readbuffer.h"
#include "alarm.h"
#include "w3m.h"
#include "http_request.h"
#include "linein.h"
#include "search.h"
#include "proto.h"
#include "buffer.h"
#include "cookie.h"
#include "downloadlist.h"
#include "funcname1.h"
#include "istream.h"
#include "func.h"
#include "keyvalue.h"
#include "form.h"
#include "history.h"
#include "anchor.h"
#include "mailcap.h"
#include "local_cgi.h"
#include "signal_util.h"
#include "display.h"
#include "terms.h"
#include "myctype.h"
#include "regex.h"
#include "rc.h"
#include "util.h"
#include "proto.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <stdlib.h>
#include <gc.h>

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume and upstream
// PHP does this as well.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

// #define HOST_NAME_MAX 255
#define MAXIMUM_COLS 1024

#define DSTR_LEN 256

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

#define HAVE_GETCWD 1
char *currentdir() {
  char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
  path = (char *)NewAtom_N(char, MAXPATHLEN);
  getcwd(path, MAXPATHLEN);
#else
  path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
  path = NewAtom_N(char, 1024);
  getwd(path);
#else  /* not HAVE_GETWD */
  FILE *f;
  char *p;
  path = (char *)NewAtom_N(char, 1024);
  f = popen("pwd", "r");
  fgets(path, 1024, f);
  pclose(f);
  for (p = path; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
  return path;
}

int show_params_p = 0;
void show_params(FILE *fp);

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void fversion(FILE *f) {
  fprintf(f, "w3m version %s, options %s\n", w3m_version,
#if LANG == JA
          "lang=ja"
#else
          "lang=en"
#endif
          ",cookie"
          ",ssl"
          ",ssl-verify"
#ifdef INET6
          ",ipv6"
#endif
          ",alarm");
}

static void fusage(FILE *f, int err) {
  fversion(f);
  fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
  fprintf(f, "    -t tab           set tab width\n");
  fprintf(f, "    -r               ignore backspace effect\n");
  fprintf(f, "    -l line          # of preserved line (default 10000)\n");
  fprintf(f, "    -B               load bookmark\n");
  fprintf(f, "    -bookmark file   specify bookmark file\n");
  fprintf(f, "    -T type          specify content-type\n");
  fprintf(f, "    -m               internet message mode\n");
  fprintf(f, "    -v               visual startup mode\n");
  fprintf(f, "    -N               open URL of command line on each new tab\n");
  fprintf(f, "    -F               automatically render frames\n");
  fprintf(f, "    -cols width      specify column width (used with -dump)\n");
  fprintf(f, "    -ppc count       specify the number of pixels per character "
             "(4.0...32.0)\n");
  fprintf(f, "    -dump            dump formatted page into stdout\n");
  fprintf(f,
          "    -dump_head       dump response of HEAD request into stdout\n");
  fprintf(f, "    -dump_source     dump page source into stdout\n");
  fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
  fprintf(f, "    -dump_extra      dump HEAD, source, and extra information "
             "into stdout\n");
  fprintf(f, "    -post file       use POST method with file content\n");
  fprintf(f, "    -header string   insert string as a header\n");
  fprintf(f, "    +<num>           goto <num> line\n");
  fprintf(f, "    -num             show line number\n");
  fprintf(f, "    -no-proxy        don't use proxy\n");
#ifdef INET6
  fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
  fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
#endif
  fprintf(f, "    -insecure        use insecure SSL config options\n");
  fprintf(f,
          "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
  fprintf(f, "    -graph           use DEC special graphics for border of "
             "table and menu\n");
  fprintf(f, "    -no-graph        use ASCII character for border of table and "
             "menu\n");
#if 1 /* pager requires -s */
  fprintf(f, "    -s               squeeze multiple blank lines\n");
#else
  fprintf(f, "    -S               squeeze multiple blank lines\n");
#endif
  fprintf(f, "    -W               toggle search wrap mode\n");
  fprintf(f, "    -X               don't use termcap init/deinit\n");
  fprintf(f, "    -title[=TERM]    set buffer name to terminal title string\n");
  fprintf(f, "    -o opt=value     assign value to config option\n");
  fprintf(f, "    -show-option     print all config options\n");
  fprintf(f, "    -config file     specify config file\n");
  fprintf(f, "    -debug           use debug mode (only for debugging)\n");
  fprintf(f, "    -reqlog          write request logfile\n");
  fprintf(f, "    -help            print this usage message\n");
  fprintf(f, "    -version         print w3m version\n");
  if (show_params_p)
    show_params(f);
  exit(err);
}

static GC_warn_proc orig_GC_warn_proc = nullptr;
#define GC_WARN_KEEP_MAX (20)

static void wrap_GC_warn_proc(char *msg, GC_word arg) {
  if (fmInitialized) {
    /* *INDENT-OFF* */
    static struct {
      char *msg;
      GC_word arg;
    } msg_ring[GC_WARN_KEEP_MAX];
    /* *INDENT-ON* */
    static int i = 0;
    static size_t n = 0;
    static int lock = 0;
    int j;

    j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
    msg_ring[j].msg = msg;
    msg_ring[j].arg = arg;

    if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
      ++n;
    else
      ++i;

    if (!lock) {
      lock = 1;

      for (; n > 0; --n, ++i) {
        i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

        printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
        sleep_till_anykey(1, 1);
      }

      lock = 0;
    }
  } else if (orig_GC_warn_proc)
    orig_GC_warn_proc(msg, arg);
  else
    fprintf(stderr, msg, (unsigned long)arg);
}

static void sig_chld(int signo) {
  int p_stat;
  pid_t pid;

  while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
    DownloadList *d;

    if (WIFEXITED(p_stat)) {
      for (d = FirstDL; d != nullptr; d = d->next) {
        if (d->pid == pid) {
          d->err = WEXITSTATUS(p_stat);
          break;
        }
      }
    }
  }
  mySignal(SIGCHLD, sig_chld);
  return;
}

static Str *make_optional_header_string(char *s) {
  char *p;
  Str *hs;

  if (strchr(s, '\n') || strchr(s, '\r'))
    return nullptr;
  for (p = s; *p && *p != ':'; p++)
    ;
  if (*p != ':' || p == s)
    return nullptr;
  hs = Strnew_size(strlen(s) + 3);
  Strcopy_charp_n(hs, s, p - s);
  if (!Strcasecmp_charp(hs, "content-type"))
    override_content_type = true;
  if (!Strcasecmp_charp(hs, "user-agent"))
    override_user_agent = true;
  Strcat_charp(hs, ": ");
  if (*(++p)) {     /* not null header */
    SKIP_BLANKS(p); /* skip white spaces */
    Strcat_charp(hs, p);
  }
  Strcat_charp(hs, "\r\n");
  return hs;
}

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
  /*
   * Suppress compiler warning: function might return no value
   * This code is never reached.
   */
  return nullptr;
}

void pcmap(void) {}

void escdmap(char c) {
  int d;
  d = (int)c - (int)'0';
  c = getch();
  if (IS_DIGIT(c)) {
    d = d * 10 + (int)c - (int)'0';
    c = getch();
  }
  if (c == '~')
    escKeyProc((int)d, K_ESCD, EscDKeymap);
}

static void SigPipe(SIGNAL_ARG) {
  mySignal(SIGPIPE, SigPipe);
  SIGNAL_RETURN;
}

static std::shared_ptr<Buffer> loadNormalBuf(const std::shared_ptr<Buffer> &buf,
                                             int renderframe) {
  CurrentTab->pushBuffer(buf);
  return buf;
}

std::shared_ptr<Buffer> loadLink(const char *url, const char *target,
                                 const char *referer, FormList *request) {
  message(Sprintf("loading %s", url)->ptr, 0, 0);
  refresh(term_io());

  const int *no_referer_ptr = nullptr;
  auto base = Currentbuf->info->getBaseURL();
  if ((no_referer_ptr && *no_referer_ptr) || !base ||
      base->schema == SCM_LOCAL || base->schema == SCM_LOCAL_CGI ||
      base->schema == SCM_DATA)
    referer = NO_REFERER;
  if (referer == nullptr)
    referer = Strnew(Currentbuf->info->currentURL.to_RefererStr())->ptr;

  auto res = loadGeneralFile(url, Currentbuf->info->getBaseURL(),
                             {.referer = referer}, request);
  if (!res) {
    char *emsg = Sprintf("Can't load %s", url)->ptr;
    disp_err_message(emsg, false);
    return nullptr;
  }

  auto buf = Buffer::create(INIT_BUFFER_WIDTH());
  buf->info = res;

  auto pu = urlParse(url, base);
  pushHashHist(URLHist, pu.to_Str().c_str());

  // if (buf == NO_BUFFER) {
  //   return nullptr;
  // }
  if (!on_target) /* open link as an indivisual page */
    return loadNormalBuf(buf, true);

  // if (do_download) /* download (thus no need to render frames) */
  //   return loadNormalBuf(buf, false);

  if (target == nullptr || /* no target specified (that means this page is not a
                           frame page) */
      !strcmp(target, "_top") /* this link is specified to be opened as an
                                 indivisual * page */
  ) {
    return loadNormalBuf(buf, true);
  }
  auto nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
  if (nfbuf == nullptr) {
    /* original page (that contains <frameset> tag) doesn't exist */
    return loadNormalBuf(buf, true);
  }

  {
    Anchor *al = nullptr;
    auto label = pu.label;

    if (!al) {
      label = Strnew_m_charp("_", target, nullptr)->ptr;
      al = Currentbuf->layout.searchURLLabel(label.c_str());
    }
    if (al) {
      Currentbuf->layout.gotoLine(al->start.line);
      if (label_topline)
        Currentbuf->layout.topLine = Currentbuf->layout.lineSkip(
            Currentbuf->layout.topLine,
            Currentbuf->layout.currentLine->linenumber -
                Currentbuf->layout.topLine->linenumber,
            false);
      Currentbuf->layout.pos = al->start.pos;
      Currentbuf->layout.arrangeCursor();
    }
  }
  displayBuffer(Currentbuf, B_NORMAL);
  return buf;
}

/* follow HREF link in the buffer */
void bufferA(void) {
  on_target = false;
  followA();
  on_target = true;
}

static FormItemList *save_submit_formlist(FormItemList *src) {
  FormList *list;
  FormList *srclist;
  FormItemList *srcitem;
  FormItemList *item;
  FormItemList *ret = nullptr;

  if (src == nullptr)
    return nullptr;
  srclist = src->parent;
  list = (FormList *)New(FormList);
  list->method = srclist->method;
  list->action = srclist->action->Strdup();
  list->enctype = srclist->enctype;
  list->nitems = srclist->nitems;
  list->body = srclist->body;
  list->boundary = srclist->boundary;
  list->length = srclist->length;

  for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
    item = (FormItemList *)New(FormItemList);
    item->type = srcitem->type;
    item->name = srcitem->name->Strdup();
    item->value = srcitem->value->Strdup();
    item->checked = srcitem->checked;
    item->accept = srcitem->accept;
    item->size = srcitem->size;
    item->rows = srcitem->rows;
    item->maxlength = srcitem->maxlength;
    item->readonly = srcitem->readonly;
    item->parent = list;
    item->next = nullptr;

    if (list->lastitem == nullptr) {
      list->item = list->lastitem = item;
    } else {
      list->lastitem->next = item;
      list->lastitem = item;
    }

    if (srcitem == src)
      ret = item;
  }

  return ret;
}

/* process form */
void followForm(void) { _followForm(false); }

static void do_submit(FormItemList *fi, Anchor *a) {
  auto tmp = Strnew();
  auto multipart = (fi->parent->method == FORM_METHOD_POST &&
                    fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
  query_from_followform(&tmp, fi, multipart);

  auto tmp2 = fi->parent->action->Strdup();
  if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
    /* It means "current URL" */
    tmp2 = Strnew(Currentbuf->info->currentURL.to_Str());
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
  }

  if (fi->parent->method == FORM_METHOD_GET) {
    char *p;
    if ((p = strchr(tmp2->ptr, '?')) != nullptr)
      Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    Strcat_charp(tmp2, "?");
    Strcat(tmp2, tmp);
    loadLink(tmp2->ptr, a->target, nullptr, nullptr);
  } else if (fi->parent->method == FORM_METHOD_POST) {
    if (multipart) {
      struct stat st;
      stat(fi->parent->body, &st);
      fi->parent->length = st.st_size;
    } else {
      fi->parent->body = tmp->ptr;
      fi->parent->length = tmp->length;
    }
    auto buf = loadLink(tmp2->ptr, a->target, nullptr, fi->parent);
    if (multipart) {
      unlink(fi->parent->body);
    }
    if (buf &&
        !(buf->info->redirectins.size() > 1)) { /* buf must be Currentbuf */
      /* BP_REDIRECTED means that the buffer is obtained through
       * Location: header. In this case, buf->form_submit must not be set
       * because the page is not loaded by POST method but GET method.
       */
      buf->layout.form_submit = save_submit_formlist(fi);
    }
  } else if ((fi->parent->method == FORM_METHOD_INTERNAL &&
              (!Strcmp_charp(fi->parent->action, "map") ||
               !Strcmp_charp(fi->parent->action, "none")))) { /* internal */
    do_internal(tmp2->ptr, tmp->ptr);
  } else {
    disp_err_message("Can't send form because of illegal method.", false);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void _followForm(int submit) {
  if (Currentbuf->layout.firstLine == nullptr)
    return;

  auto a = retrieveCurrentForm(&Currentbuf->layout);
  if (a == nullptr)
    return;
  auto fi = (FormItemList *)a->url;

  switch (fi->type) {
  case FORM_INPUT_TEXT:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);
    inputStrHist("TEXT:", fi->value ? fi->value->ptr : nullptr, TextHist,
                 [fi, a](const char *p) {
                   if (p == nullptr || fi->readonly) {
                     return;
                     // break;
                   }
                   fi->value = Strnew_charp(p);
                   formUpdateBuffer(a, &Currentbuf->layout, fi);
                   if (fi->accept || fi->parent->nitems == 1) {
                     do_submit(fi, a);
                     return;
                   }
                   displayBuffer(Currentbuf, B_FORCE_REDRAW);
                 });
    break;

  case FORM_INPUT_FILE:
    if (submit) {
      do_submit(fi, a);
      return;
    }

    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);

    // p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : nullptr,
    // nullptr); if (p == nullptr || fi->readonly)
    //   break;
    //
    // fi->value = Strnew_charp(p);
    // formUpdateBuffer(a, Currentbuf, fi);
    // if (fi->accept || fi->parent->nitems == 1)
    //   goto do_submit;
    break;
  case FORM_INPUT_PASSWORD:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    // p = inputLine("Password:", fi->value ? fi->value->ptr : nullptr,
    // IN_PASSWORD);
    // if (p == nullptr)
    //   break;
    // fi->value = Strnew_charp(p);
    formUpdateBuffer(a, &Currentbuf->layout, fi);
    if (fi->accept) {
      do_submit(fi, a);
      return;
    }
    break;
  case FORM_TEXTAREA:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly)
      disp_message_nsec("Read only field!", false, 1, true, false);
    input_textarea(fi);
    formUpdateBuffer(a, &Currentbuf->layout, fi);
    break;
  case FORM_INPUT_RADIO:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    formRecheckRadio(a, Currentbuf, fi);
    break;
  case FORM_INPUT_CHECKBOX:
    if (submit) {
      do_submit(fi, a);
      return;
    }
    if (fi->readonly) {
      disp_message_nsec("Read only field!", false, 1, true, false);
      break;
    }
    fi->checked = !fi->checked;
    formUpdateBuffer(a, &Currentbuf->layout, fi);
    break;
  case FORM_INPUT_IMAGE:
  case FORM_INPUT_SUBMIT:
  case FORM_INPUT_BUTTON:
    do_submit(fi, a);
    break;

  case FORM_INPUT_RESET:
    for (size_t i = 0; i < Currentbuf->layout.formitem->size(); i++) {
      auto a2 = &Currentbuf->layout.formitem->anchors[i];
      auto f2 = (FormItemList *)a2->url;
      if (f2->parent == fi->parent && f2->name && f2->value &&
          f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN &&
          f2->type != FORM_INPUT_RESET) {
        f2->value = f2->init_value;
        f2->checked = f2->init_checked;
        formUpdateBuffer(a2, &Currentbuf->layout, f2);
      }
    }
    break;
  case FORM_INPUT_HIDDEN:
  default:
    break;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(const std::shared_ptr<Buffer> &buf) {
  static const char *url_like_pat[] = {
      "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/"
      "=\\-]",
      "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
      "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifndef USE_W3MMAILER /* see also chkExternalURIBuffer() */
      "mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
      "https?://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./"
      "?=~_\\&+@#,\\$;]*",
      "ftp://[a-zA-Z0-9:%\\-\\./"
      "_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif /* INET6 */
      nullptr};
  int i;
  for (i = 0; url_like_pat[i]; i++) {
    reAnchor(&buf->layout, url_like_pat[i]);
  }
  buf->check_url = true;
}

void deleteFiles() {
  for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
    while (Firstbuf /*&& Firstbuf != NO_BUFFER*/) {
      auto buf = Firstbuf->backBuffer;
      discardBuffer(Firstbuf);
      Firstbuf = buf;
    }
  }
  while (auto f = popText(fileToDelete)) {
    unlink(f);
  }
}

void w3m_exit(int i) {
  stopDownload();
  deleteFiles();
  free_ssl_ctx();
  if (mkd_tmp_dir)
    if (rmdir(mkd_tmp_dir) != 0) {
      fprintf(stderr, "Can't remove temporary directory (%s)!\n", mkd_tmp_dir);
      exit(1);
    }
  exit(i);
}

AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, int cmd,
                          void *data) {
  if (event == nullptr)
    event = (AlarmEvent *)New(AlarmEvent);
  event->sec = sec;
  event->status = status;
  event->cmd = cmd;
  event->data = data;
  return event;
}

void download_action(struct keyvalue *arg) {
  DownloadList *d;
  pid_t pid;

  for (; arg; arg = arg->next) {
    if (!strncmp(arg->arg, "stop", 4)) {
      pid = (pid_t)atoi(&arg->arg[4]);
      kill(pid, SIGKILL);
    } else if (!strncmp(arg->arg, "ok", 2))
      pid = (pid_t)atoi(&arg->arg[2]);
    else
      continue;
    for (d = FirstDL; d; d = d->next) {
      if (d->pid == pid) {
        unlink(d->lock);
        if (d->prev)
          d->prev->next = d->next;
        else
          FirstDL = d->next;
        if (d->next)
          d->next->prev = d->prev;
        else
          LastDL = d->prev;
        break;
      }
    }
  }
  ldDL();
}

void stopDownload(void) {
  DownloadList *d;

  if (!FirstDL)
    return;
  for (d = FirstDL; d != nullptr; d = d->next) {
    if (!d->running)
      continue;
    kill(d->pid, SIGKILL);
    unlink(d->lock);
  }
}

int main(int argc, char **argv) {
  std::shared_ptr<Buffer> newbuf;
  char *p;
  int i;
  char *line_str = nullptr;
  const char **load_argv;
  FormList *request;
  int load_argc = 0;
  int load_bookmark = false;
  int visual_start = false;
  int open_new_tab = false;
  char *default_type = nullptr;
  char *post_file = nullptr;
  Str *err_msg;
  if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
    set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif

  fileToDelete = newTextList();

  load_argv = (const char **)New_N(char *, argc - 1);
  load_argc = 0;

  CurrentDir = currentdir();
  CurrentPid = (int)getpid();
  BookmarkFile = nullptr;
  config_file = nullptr;

  {
    char hostname[HOST_NAME_MAX + 2];
    if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
      size_t hostname_len;
      /* Don't use hostname if it is truncated.  */
      hostname[HOST_NAME_MAX + 1] = '\0';
      hostname_len = strlen(hostname);
      if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX)
        HostName = allocStr(hostname, (int)hostname_len);
    }
  }

  /* argument search 1 */
  for (i = 1; i < argc; i++) {
    if (*argv[i] == '-') {
      if (!strcmp("-config", argv[i])) {
        argv[i] = (char *)"-dummy";
        if (++i >= argc)
          usage();
        config_file = argv[i];
        argv[i] = (char *)"-dummy";
      } else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
        help();
      else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
        fversion(stdout);
        exit(0);
      }
    }
  }

  /* initializations */
  init_rc();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();

  if (!non_null(Editor) && (p = getenv("EDITOR")) != nullptr)
    Editor = p;
  if (!non_null(Mailer) && (p = getenv("MAILER")) != nullptr)
    Mailer = p;

  /* argument search 2 */
  i = 1;
  while (i < argc) {
    if (*argv[i] == '-') {
      if (!strcmp("-t", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          Tabstop = atoi(argv[i]);
      } else if (!strcmp("-l", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          PagerMax = atoi(argv[i]);
      } else if (!strcmp("-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_DEC;
      else if (!strcmp("-no-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_ASCII;
      else if (!strcmp("-T", argv[i])) {
        if (++i >= argc)
          usage();
        DefaultType = default_type = argv[i];
      } else if (!strcmp("-v", argv[i]))
        visual_start = true;
      else if (!strcmp("-N", argv[i]))
        open_new_tab = true;
      else if (!strcmp("-B", argv[i]))
        load_bookmark = true;
      else if (!strcmp("-bookmark", argv[i])) {
        if (++i >= argc)
          usage();
        BookmarkFile = argv[i];
        if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
          Str *tmp = Strnew_charp(CurrentDir);
          if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
          Strcat_charp(tmp, BookmarkFile);
          BookmarkFile = cleanupName(tmp->ptr);
        }
      } else if (!strcmp("-W", argv[i])) {
        if (WrapDefault)
          WrapDefault = false;
        else
          WrapDefault = true;
      } else if (!strcmp("-cols", argv[i])) {
        if (++i >= argc)
          usage();
        COLS = atoi(argv[i]);
        if (COLS > MAXIMUM_COLS) {
          COLS = MAXIMUM_COLS;
        }
      } else if (!strcmp("-ppc", argv[i])) {
        double ppc;
        if (++i >= argc)
          usage();
        ppc = atof(argv[i]);
        if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
          pixel_per_char = ppc;
          set_pixel_per_char = true;
        }
      } else if (!strcmp("-num", argv[i])) {
        showLineNum = true;
      } else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i])) {
        set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
      } else if (!strcmp("-post", argv[i])) {
        if (++i >= argc)
          usage();
        post_file = argv[i];
      } else if (!strcmp("-header", argv[i])) {
        Str *hs;
        if (++i >= argc)
          usage();
        if ((hs = make_optional_header_string(argv[i])) != nullptr) {
          if (header_string == nullptr)
            header_string = hs;
          else
            Strcat(header_string, hs);
        }
        while (argv[i][0]) {
          argv[i][0] = '\0';
          argv[i]++;
        }
      } else if (!strcmp("-no-cookie", argv[i])) {
        use_cookie = false;
        accept_cookie = false;
      } else if (!strcmp("-cookie", argv[i])) {
        use_cookie = true;
        accept_cookie = true;
      }
#if 1 /* pager requires -s */
      else if (!strcmp("-s", argv[i]))
#else
      else if (!strcmp("-S", argv[i]))
#endif
        squeezeBlankLine = true;
      else if (!strcmp("-X", argv[i]))
        Do_not_use_ti_te = true;
      else if (!strcmp("-title", argv[i]))
        displayTitleTerm = getenv("TERM");
      else if (!strncmp("-title=", argv[i], 7))
        displayTitleTerm = argv[i] + 7;
      else if (!strcmp("-insecure", argv[i])) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
        set_param_option("ssl_cipher=ALL:enullptr:@SECLEVEL=0");
#else
        set_param_option("ssl_cipher=ALL:enullptr");
#endif
#ifdef SSL_CTX_set_min_proto_version
        set_param_option("ssl_min_version=all");
#endif
        set_param_option("ssl_forbid_method=");
        set_param_option("ssl_verify_server=0");
      } else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i])) {
        if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
            !strcmp(argv[i], "?")) {
          show_params(stdout);
          exit(0);
        }
        if (!set_param_option(argv[i])) {
          /* option set failed */
          /* FIXME: gettextize? */
          fprintf(stderr, "%s: bad option\n", argv[i]);
          show_params_p = 1;
          usage();
        }
      } else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i])) {
        /* do nothing */
      } else if (!strcmp("-reqlog", argv[i])) {
        w3m_reqlog = rcFile("request.log");
      } else {
        usage();
      }
    } else if (*argv[i] == '+') {
      line_str = argv[i] + 1;
    } else {
      load_argv[load_argc++] = argv[i];
    }
    i++;
  }

  FirstTab = nullptr;
  LastTab = nullptr;
  nTab = 0;
  CurrentTab = nullptr;
  CurrentKey = -1;
  if (BookmarkFile == nullptr)
    BookmarkFile = rcFile(BOOKMARK);

  fmInit();
  mySignal(SIGWINCH, resize_hook);

  sync_with_option();
  initCookie();
  if (UseHistory)
    loadHistory(URLHist);

  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif
  err_msg = Strnew();
  if (load_argc == 0) {
    /* no URL specified */
    if (load_bookmark) {
      auto res = loadGeneralFile(BookmarkFile, {}, {.referer = NO_REFERER});
      if (res) {
        newbuf = Buffer::create(INIT_BUFFER_WIDTH());
        newbuf->info = res;
      } else {
        Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
      }
    } else if (visual_start) {
      Str *s_page;
      s_page =
          Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
      Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
      Strcat_m_charp(
          s_page, "w3m</a>!<p><p>This is w3m version ", w3m_version,
          "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
          nullptr);
      newbuf = loadHTMLString(s_page);
      if (newbuf == nullptr)
        Strcat_charp(err_msg, "w3m: Can't load string.\n");
    } else if ((p = getenv("HTTP_HOME")) != nullptr ||
               (p = getenv("WWW_HOME")) != nullptr) {
      auto res = loadGeneralFile(p, {}, {.referer = NO_REFERER});
      if (res) {
        newbuf = Buffer::create(INIT_BUFFER_WIDTH());
        newbuf->info = res;
        // if (newbuf != NO_BUFFER)
        { pushHashHist(URLHist, newbuf->info->currentURL.to_Str().c_str()); }
      } else {
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
      }
    } else {
      if (fmInitialized)
        fmTerm();
      usage();
    }
    if (newbuf == nullptr) {
      if (fmInitialized)
        fmTerm();
      if (err_msg->length)
        fprintf(stderr, "%s", err_msg->ptr);
      w3m_exit(2);
    }
    i = -1;
  } else {
    i = 0;
  }
  for (; i < load_argc; i++) {
    if (i >= 0) {
      DefaultType = default_type;
      int retry = 0;

      auto url = load_argv[i];
      if (parseUrlSchema(&url) == SCM_MISSING && !ArgvIsURL)
      retry_as_local_file:
        url = file_to_url((char *)load_argv[i]);
      else
        url = Strnew(url_quote(load_argv[i]))->ptr;
      {
        if (post_file && i == 0) {
          FILE *fp;
          Str *body;
          if (!strcmp(post_file, "-"))
            fp = stdin;
          else
            fp = fopen(post_file, "r");
          if (fp == nullptr) {
            Strcat(err_msg, Sprintf("w3m: Can't open %s.\n", post_file));
            continue;
          }
          body = Strfgetall(fp);
          if (fp != stdin)
            fclose(fp);
          request = newFormList(nullptr, "post", nullptr, nullptr, nullptr,
                                nullptr, nullptr);
          request->body = body->ptr;
          request->boundary = nullptr;
          request->length = body->length;
        } else {
          request = nullptr;
        }
        auto res = loadGeneralFile(url, {}, {.referer = NO_REFERER}, request);
        if (res) {
          newbuf = Buffer::create(INIT_BUFFER_WIDTH());
          newbuf->info = res;
        }
      }
      if (newbuf == nullptr) {
        if (ArgvIsURL && !retry) {
          retry = 1;
          goto retry_as_local_file;
        }
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", load_argv[i]));
        continue;
      }
      // else if (newbuf == NO_BUFFER) {
      //   continue;
      // }

      switch (newbuf->info->currentURL.schema) {
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
      default:
        pushHashHist(URLHist, newbuf->info->currentURL.to_Str().c_str());
        break;
      }
    }
    // else if (newbuf == NO_BUFFER)
    //   continue;

    if (!CurrentTab) {
      TabBuffer::init(newbuf);
    } else if (open_new_tab) {
      TabBuffer::_newT();
      Currentbuf->backBuffer = newbuf;
      CurrentTab->deleteBuffer(Currentbuf);
    } else {
      Currentbuf->backBuffer = newbuf;
      CurrentTab->currentBuffer(newbuf);
    }

    {
      CurrentTab->currentBuffer(newbuf);
      saveBufferInfo();
    }
  }

  if (popAddDownloadList()) {
    CurrentTab = LastTab;
    if (!FirstTab) {
      FirstTab = LastTab = CurrentTab = new TabBuffer();
      nTab = 1;
    }
    if (!Firstbuf /*|| Firstbuf == NO_BUFFER*/) {
      CurrentTab->currentBuffer(Buffer::create(INIT_BUFFER_WIDTH()), true);
      Currentbuf->layout.title = DOWNLOAD_LIST_TITLE;
    } else {
      CurrentTab->currentBuffer(Firstbuf);
    }
    ldDL();
  } else
    CurrentTab = FirstTab;
  if (!FirstTab || !Firstbuf /*|| Firstbuf == NO_BUFFER*/) {
    // if (newbuf == NO_BUFFER) {
    //   if (fmInitialized) {
    //     // inputChar("Hit any key to quit w3m:");
    //   }
    // }
    if (fmInitialized)
      fmTerm();
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    // if (newbuf == NO_BUFFER) {
    //   save_cookies();
    //   if (!err_msg->length)
    //     w3m_exit(0);
    // }
    w3m_exit(2);
  }
  if (err_msg->length)
    disp_message_nsec(err_msg->ptr, false, 1, true, false);

  DefaultType = nullptr;

  CurrentTab->currentBuffer(Firstbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  if (line_str) {
    _goLine(line_str);
  }

  mainLoop();
}
