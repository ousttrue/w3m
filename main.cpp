/* $Id: main.c,v 1.270 2010/08/24 10:11:51 htrb Exp $ */
#define MAINPROGRAM
#include "Args.h"
extern "C" {

#include "core.h"
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif

int _main(int argc, char **argv);
}

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

const auto PREC_LIMIT = 10000;

static GC_warn_proc orig_GC_warn_proc = NULL;
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
    static int n = 0;
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

#ifdef HAVE_WAITPID
  while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0)
#elif HAVE_WAIT3
  while ((pid = wait3(&p_stat, WNOHANG, NULL)) > 0)
#else
  if ((pid = wait(&p_stat)) > 0)
#endif
  {
    DownloadList *d;

    if (WIFEXITED(p_stat)) {
      for (d = FirstDL; d != NULL; d = d->next) {
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

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
}

static void initialize() {
  if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL"))
    set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif
#if defined(ENABLE_NLS) || (defined(USE_M17N) && defined(HAVE_LANGINFO_CODESET))
  setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif

  NO_proxy_domains = newTextList();
  fileToDelete = newTextList();

  CurrentDir = currentdir();
  CurrentPid = (int)getpid();
  BookmarkFile = NULL;
  config_file = NULL;

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
}

int _main(int argc, char **argv) {
  initialize();
  help_or_exit(argc, argv);

  char *Locale = NULL;
  if (non_null(Locale = getenv("LC_ALL")) ||
      non_null(Locale = getenv("LC_CTYPE")) ||
      non_null(Locale = getenv("LANG"))) {
    DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
    DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
    SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
  }

  // initializations
  init_rc();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();

  if (FollowLocale && Locale) {
    DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
    SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
  }

  wc_uint8 auto_detect = WcOption.auto_detect;
  BookmarkCharset = DocumentCharset;

  char *p;
  if (!non_null(HTTP_proxy) &&
      ((p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy")) ||
       (p = getenv("HTTP_proxy"))))
    HTTP_proxy = p;
  if (!non_null(HTTPS_proxy) &&
      ((p = getenv("HTTPS_PROXY")) || (p = getenv("https_proxy")) ||
       (p = getenv("HTTPS_proxy"))))
    HTTPS_proxy = p;
  if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
    HTTPS_proxy = HTTP_proxy;
  if (!non_null(FTP_proxy) &&
      ((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) ||
       (p = getenv("FTP_proxy"))))
    FTP_proxy = p;
  if (!non_null(NO_proxy) &&
      ((p = getenv("NO_PROXY")) || (p = getenv("no_proxy")) ||
       (p = getenv("NO_proxy"))))
    NO_proxy = p;

  if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
    Editor = p;
  if (!non_null(Mailer) && (p = getenv("MAILER")) != NULL)
    Mailer = p;

  /* argument search 2 */
  auto load_argv = New_N(char *, argc - 1);

  Args argument;
  auto i = argument.parse(argc, argv, load_argv);

  FirstTab = NULL;
  LastTab = NULL;
  nTab = 0;
  CurrentTab = NULL;
  CurrentKey = -1;
  if (BookmarkFile == NULL)
    BookmarkFile = rcFile(BOOKMARK);

  if (!isatty(1) && !w3m_dump) {
    /* redirected output */
    w3m_dump = DUMP_BUFFER;
  }
  if (w3m_dump) {
    if (COLS == 0)
      COLS = DEFAULT_COLS;
  }

  if (!w3m_dump && !w3m_backend) {
    fmInit();
    mySignal(SIGWINCH, resize_hook);
  } else if (w3m_halfdump && displayImage)
    activeImage = TRUE;

  sync_with_option();
  initCookie();
  if (UseHistory)
    loadHistory(URLHist);

  wtf_init(DocumentCharset, DisplayCharset);
  /*  if (w3m_dump)
   *    WcOption.pre_conv = WC_TRUE;
   */

  if (w3m_backend)
    backend();

  if (w3m_dump)
    mySignal(SIGINT, SIG_IGN);
  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif

  Str err_msg = Strnew();
  Buffer *newbuf = NULL;
  InputStream redin;
  if (argument.load_argc == 0) {
    // no URL specified
    if (!isatty(0)) {
      redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
      newbuf = openGeneralPagerBuffer(redin);
      dup2(1, 0);
    } else if (argument.load_bookmark) {
      newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
      if (newbuf == NULL)
        Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
    } else if (argument.visual_start) {
      /* FIXME: gettextize? */
      Str s_page;
      s_page =
          Strnew_charp("<title>W3M startup page</title><center><b>Welcome to ");
      Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
      Strcat_m_charp(
          s_page, "w3m</a>!<p><p>This is w3m version ", w3m_version,
          "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
          NULL);
      newbuf = loadHTMLString(s_page);
      if (newbuf == NULL)
        Strcat_charp(err_msg, "w3m: Can't load string.\n");
      else if (newbuf != NO_BUFFER)
        newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    } else if ((p = getenv("HTTP_HOME")) != NULL ||
               (p = getenv("WWW_HOME")) != NULL) {
      newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
      if (newbuf == NULL)
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
      else if (newbuf != NO_BUFFER)
        pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
    } else {
      if (fmInitialized)
        fmTerm();
      usage();
    }
    if (newbuf == NULL) {
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

  FormList *request;
  for (; i < argument.load_argc; i++) {
    if (i >= 0) {
      SearchHeader = argument.search_header;
      DefaultType = argument.default_type;
      char *url;

      url = load_argv[i];
      if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
        url = file_to_url(load_argv[i]);
      else
        url = url_encode(conv_from_system(load_argv[i]), NULL, 0);
      if (w3m_dump == DUMP_HEAD) {
        request = New(FormList);
        request->method = FORM_METHOD_HEAD;
        newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
      } else {
        if (argument.post_file && i == 0) {
          FILE *fp;
          Str body;
          if (!strcmp(argument.post_file, "-"))
            fp = stdin;
          else
            fp = fopen(argument.post_file, "r");
          if (fp == NULL) {
            /* FIXME: gettextize? */
            Strcat(err_msg,
                   Sprintf("w3m: Can't open %s.\n", argument.post_file));
            continue;
          }
          body = Strfgetall(fp);
          if (fp != stdin)
            fclose(fp);
          request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
          request->body = body->ptr;
          request->boundary = NULL;
          request->length = body->length;
        } else {
          request = NULL;
        }
        newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
      }
      if (newbuf == NULL) {
        /* FIXME: gettextize? */
        Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", load_argv[i]));
        continue;
      } else if (newbuf == NO_BUFFER)
        continue;
      switch (newbuf->real_scheme) {
      case SCM_MAILTO:
        break;
      case SCM_LOCAL:
      case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
      default:
        pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
        break;
      }
    } else if (newbuf == NO_BUFFER)
      continue;
    if (newbuf->pagerSource ||
        (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source &&
         newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
      newbuf->search_header = argument.search_header;
    if (CurrentTab == NULL) {
      FirstTab = LastTab = CurrentTab = newTab();
      nTab = 1;
      Firstbuf = Currentbuf = newbuf;
    } else if (argument.open_new_tab) {
      _newT();
      Currentbuf->nextBuffer = newbuf;
      delBuffer(Currentbuf);
    } else {
      Currentbuf->nextBuffer = newbuf;
      Currentbuf = newbuf;
    }
    if (!w3m_dump || w3m_dump == DUMP_BUFFER) {
      if (Currentbuf->frameset != NULL && RenderFrame)
        rFrame();
    }
    if (w3m_dump)
      do_dump(Currentbuf);
    else {
      Currentbuf = newbuf;
      saveBufferInfo();
    }
  }
  if (w3m_dump) {
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    save_cookies();
    w3m_exit(0);
  }

  if (add_download_list) {
    add_download_list = FALSE;
    CurrentTab = LastTab;
    if (!FirstTab) {
      FirstTab = LastTab = CurrentTab = newTab();
      nTab = 1;
    }
    if (!Firstbuf || Firstbuf == NO_BUFFER) {
      Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
      Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
      Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
    } else
      Currentbuf = Firstbuf;
    ldDL();
  } else
    CurrentTab = FirstTab;
  if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
    if (newbuf == NO_BUFFER) {
      if (fmInitialized)
        /* FIXME: gettextize? */
        inputChar("Hit any key to quit w3m:");
    }
    if (fmInitialized)
      fmTerm();
    if (err_msg->length)
      fprintf(stderr, "%s", err_msg->ptr);
    if (newbuf == NO_BUFFER) {
      save_cookies();
      if (!err_msg->length)
        w3m_exit(0);
    }
    w3m_exit(2);
  }
  if (err_msg->length)
    disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

  SearchHeader = FALSE;
  DefaultType = NULL;
  UseContentCharset = TRUE;
  WcOption.auto_detect = auto_detect;

  Currentbuf = Firstbuf;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  if (argument.line_str) {
    _goLine(argument.line_str);
  }

  //
  // main loop
  //
  for (;;) {
    if (add_download_list) {
      add_download_list = FALSE;
      ldDL();
    }
    if (Currentbuf->submit) {
      Anchor *a = Currentbuf->submit;
      Currentbuf->submit = NULL;
      gotoLine(Currentbuf, a->start.line);
      Currentbuf->pos = a->start.pos;
      _followForm(TRUE);
      continue;
    }
    /* event processing */
    if (CurrentEvent) {
      CurrentKey = -1;
      CurrentKeyData = NULL;
      CurrentCmdData = (char *)CurrentEvent->data;
      w3mFuncList[CurrentEvent->cmd].func();
      CurrentCmdData = NULL;
      CurrentEvent = CurrentEvent->next;
      continue;
    }
    /* get keypress event */
    if (Currentbuf->event) {
      if (Currentbuf->event->status != AL_UNSET) {
        CurrentAlarm = Currentbuf->event;
        if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
          Currentbuf->event = NULL;
          CurrentKey = -1;
          CurrentKeyData = NULL;
          CurrentCmdData = (char *)CurrentAlarm->data;
          w3mFuncList[CurrentAlarm->cmd].func();
          CurrentCmdData = NULL;
          continue;
        }
      } else
        Currentbuf->event = NULL;
    }
    if (!Currentbuf->event)
      CurrentAlarm = &DefaultAlarm;
    if (CurrentAlarm->sec > 0) {
      mySignal(SIGALRM, SigAlarm);
      alarm(CurrentAlarm->sec);
    }
    mySignal(SIGWINCH, resize_hook);
    if (activeImage && displayImage && Currentbuf->img &&
        !Currentbuf->image_loaded) {
      do {
        if (need_resize_screen)
          resize_screen();
        loadImage(Currentbuf, IMG_FLAG_NEXT);
      } while (sleep_till_anykey(1, 0) <= 0);
    } else {
      do {
        if (need_resize_screen)
          resize_screen();
      } while (sleep_till_anykey(1, 0) <= 0);
    }
    int c = getch();
    if (CurrentAlarm->sec > 0) {
      alarm(0);
    }
    if (IS_ASCII(c)) { /* Ascii */
      if (('0' <= c) && (c <= '9') &&
          (prec_num || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
        prec_num = prec_num * 10 + (int)(c - '0');
        if (prec_num > PREC_LIMIT)
          prec_num = PREC_LIMIT;
      } else {
        set_buffer_environ(Currentbuf);
        save_buffer_position(Currentbuf);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
  }
}
