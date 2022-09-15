/* $Id: main.c,v 1.270 2010/08/24 10:11:51 htrb Exp $ */
#define MAINPROGRAM
extern "C" {

#include "fm.h"
#include "core.h"
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <time.h>
#if defined(__CYGWIN__) && defined(USE_BINMODE_STREAM)
#include <io.h>
#endif
#include "myctype.h"
#include "regex.h"
#include "terms.h"
#include "ucs.h"
#include "wc.h"
#include "wtf.h"

int _main(int argc, char **argv);
void show_params(FILE *fp);

}

#define DSTR_LEN 256

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

int show_params_p = 0;


#define PREC_LIMIT 10000

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

int enable_inline_image;

static void fversion(FILE *f) {
  fprintf(f, "w3m version %s, options %s\n", w3m_version,
#if LANG == JA
          "lang=ja"
#else
          "lang=en"
#endif
          ",m17n"
          ",image"
          ",color"
          ",ansi-color"
          ",menu"
          ",cookie"
          ",ssl"
          ",ssl-verify"
          ",external-uri-loader"
#ifdef INET6
          ",ipv6"
#endif
          ",alarm");
}

static void fusage(FILE *f, int err) {
  fversion(f);
  /* FIXME: gettextize? */
  fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
  fprintf(f, "    -t tab           set tab width\n");
  fprintf(f, "    -r               ignore backspace effect\n");
  fprintf(f, "    -l line          # of preserved line (default 10000)\n");
  fprintf(f, "    -I charset       document charset\n");
  fprintf(f, "    -O charset       display/output charset\n");
  fprintf(f, "    -B               load bookmark\n");
  fprintf(f, "    -bookmark file   specify bookmark file\n");
  fprintf(f, "    -T type          specify content-type\n");
  fprintf(f, "    -m               internet message mode\n");
  fprintf(f, "    -v               visual startup mode\n");
  fprintf(f, "    -M               monochrome display\n");
  fprintf(f, "    -N               open URL of command line on each new tab\n");
  fprintf(f, "    -F               automatically render frames\n");
  fprintf(f, "    -cols width      specify column width (used with -dump)\n");
  fprintf(f, "    -ppc count       specify the number of pixels per character "
             "(4.0...32.0)\n");
  fprintf(f, "    -ppl count       specify the number of pixels per line "
             "(4.0...64.0)\n");
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
  fprintf(f, "    -s               squeeze multiple blank lines\n");
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

Str make_optional_header_string(char *s) {
  char *p;
  Str hs;

  if (strchr(s, '\n') || strchr(s, '\r'))
    return NULL;
  for (p = s; *p && *p != ':'; p++)
    ;
  if (*p != ':' || p == s)
    return NULL;
  hs = Strnew_size(strlen(s) + 3);
  Strcopy_charp_n(hs, s, p - s);
  if (!Strcasecmp_charp(hs, "content-type"))
    override_content_type = TRUE;
  if (!Strcasecmp_charp(hs, "user-agent"))
    override_user_agent = TRUE;
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

static void help_or_exit(int argc, char **argv) {
  /* argument search 1 */
  for (int i = 1; i < argc; i++) {
    if (*argv[i] == '-') {
      if (!strcmp("-config", argv[i])) {
        argv[i] = "-dummy";
        if (++i >= argc)
          usage();
        config_file = argv[i];
        argv[i] = "-dummy";
      } else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
        help();
      else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
        fversion(stdout);
        exit(0);
      }
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
  auto load_argv = New_N(char *, argc - 1);
  int load_argc = 0;
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
  int i = 1;
  char *line_str = NULL;
  int load_bookmark = FALSE;
  int visual_start = FALSE;
  int open_new_tab = FALSE;
  char search_header = FALSE;
  char *default_type = NULL;
  char *post_file = NULL;
  while (i < argc) {
    if (*argv[i] == '-') {
      if (!strcmp("-t", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          Tabstop = atoi(argv[i]);
      } else if (!strcmp("-r", argv[i]))
        ShowEffect = FALSE;
      else if (!strcmp("-l", argv[i])) {
        if (++i >= argc)
          usage();
        if (atoi(argv[i]) > 0)
          PagerMax = atoi(argv[i]);
      } else if (!strncmp("-I", argv[i], 2)) {
        if (argv[i][2] != '\0')
          p = argv[i] + 2;
        else {
          if (++i >= argc)
            usage();
          p = argv[i];
        }
        DocumentCharset = wc_guess_charset_short(p, DocumentCharset);
        WcOption.auto_detect = WC_OPT_DETECT_OFF;
        UseContentCharset = FALSE;
      } else if (!strncmp("-O", argv[i], 2)) {
        if (argv[i][2] != '\0')
          p = argv[i] + 2;
        else {
          if (++i >= argc)
            usage();
          p = argv[i];
        }
        DisplayCharset = wc_guess_charset_short(p, DisplayCharset);
      } else if (!strcmp("-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_DEC;
      else if (!strcmp("-no-graph", argv[i]))
        UseGraphicChar = GRAPHIC_CHAR_ASCII;
      else if (!strcmp("-T", argv[i])) {
        if (++i >= argc)
          usage();
        DefaultType = default_type = argv[i];
      } else if (!strcmp("-m", argv[i]))
        SearchHeader = search_header = TRUE;
      else if (!strcmp("-v", argv[i]))
        visual_start = TRUE;
      else if (!strcmp("-N", argv[i]))
        open_new_tab = TRUE;
      else if (!strcmp("-M", argv[i]))
        useColor = FALSE;
      else if (!strcmp("-B", argv[i]))
        load_bookmark = TRUE;
      else if (!strcmp("-bookmark", argv[i])) {
        if (++i >= argc)
          usage();
        BookmarkFile = argv[i];
        if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
          Str tmp = Strnew_charp(CurrentDir);
          if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
          Strcat_charp(tmp, BookmarkFile);
          BookmarkFile = cleanupName(tmp->ptr);
        }
      } else if (!strcmp("-F", argv[i]))
        RenderFrame = TRUE;
      else if (!strcmp("-W", argv[i])) {
        if (WrapDefault)
          WrapDefault = FALSE;
        else
          WrapDefault = TRUE;
      } else if (!strcmp("-dump", argv[i]))
        w3m_dump = DUMP_BUFFER;
      else if (!strcmp("-dump_source", argv[i]))
        w3m_dump = DUMP_SOURCE;
      else if (!strcmp("-dump_head", argv[i]))
        w3m_dump = DUMP_HEAD;
      else if (!strcmp("-dump_both", argv[i]))
        w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
      else if (!strcmp("-dump_extra", argv[i]))
        w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
      else if (!strcmp("-halfdump", argv[i]))
        w3m_dump = DUMP_HALFDUMP;
      else if (!strcmp("-halfload", argv[i])) {
        w3m_dump = 0;
        w3m_halfload = TRUE;
        DefaultType = default_type = "text/html";
      } else if (!strcmp("-backend", argv[i])) {
        w3m_backend = TRUE;
      } else if (!strcmp("-backend_batch", argv[i])) {
        w3m_backend = TRUE;
        if (++i >= argc)
          usage();
        if (!backend_batch_commands)
          backend_batch_commands = newTextList();
        pushText(backend_batch_commands, argv[i]);
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
          set_pixel_per_char = TRUE;
        }
      } else if (!strcmp("-ppl", argv[i])) {
        double ppc;
        if (++i >= argc)
          usage();
        ppc = atof(argv[i]);
        if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
            ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
          pixel_per_line = ppc;
          set_pixel_per_line = TRUE;
        }
      } else if (!strcmp("-ri", argv[i])) {
        enable_inline_image = INLINE_IMG_OSC5379;
      } else if (!strcmp("-sixel", argv[i])) {
        enable_inline_image = INLINE_IMG_SIXEL;
      } else if (!strcmp("-num", argv[i]))
        showLineNum = TRUE;
      else if (!strcmp("-no-proxy", argv[i]))
        use_proxy = FALSE;
#ifdef INET6
      else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
        set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
#endif
      else if (!strcmp("-post", argv[i])) {
        if (++i >= argc)
          usage();
        post_file = argv[i];
      } else if (!strcmp("-header", argv[i])) {
        Str hs;
        if (++i >= argc)
          usage();
        if ((hs = make_optional_header_string(argv[i])) != NULL) {
          if (header_string == NULL)
            header_string = hs;
          else
            Strcat(header_string, hs);
        }
        while (argv[i][0]) {
          argv[i][0] = '\0';
          argv[i]++;
        }
      } else if (!strcmp("-no-cookie", argv[i])) {
        use_cookie = FALSE;
        accept_cookie = FALSE;
      } else if (!strcmp("-cookie", argv[i])) {
        use_cookie = TRUE;
        accept_cookie = TRUE;
      } else if (!strcmp("-s", argv[i]))
        squeezeBlankLine = TRUE;
      else if (!strcmp("-X", argv[i]))
        Do_not_use_ti_te = TRUE;
      else if (!strcmp("-title", argv[i]))
        displayTitleTerm = getenv("TERM");
      else if (!strncmp("-title=", argv[i], 7))
        displayTitleTerm = argv[i] + 7;
      else if (!strcmp("-insecure", argv[i])) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
        set_param_option("ssl_cipher=ALL:eNULL:@SECLEVEL=0");
#else
        set_param_option("ssl_cipher=ALL:eNULL");
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
      } else if (!strcmp("-debug", argv[i])) {
        w3m_debug = TRUE;
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
  if (load_argc == 0) {
    /* no URL specified */
    if (!isatty(0)) {
      redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
      newbuf = openGeneralPagerBuffer(redin);
      dup2(1, 0);
    } else if (load_bookmark) {
      newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
      if (newbuf == NULL)
        Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
    } else if (visual_start) {
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
  for (; i < load_argc; i++) {
    if (i >= 0) {
      SearchHeader = search_header;
      DefaultType = default_type;
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
        if (post_file && i == 0) {
          FILE *fp;
          Str body;
          if (!strcmp(post_file, "-"))
            fp = stdin;
          else
            fp = fopen(post_file, "r");
          if (fp == NULL) {
            /* FIXME: gettextize? */
            Strcat(err_msg, Sprintf("w3m: Can't open %s.\n", post_file));
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
      newbuf->search_header = search_header;
    if (CurrentTab == NULL) {
      FirstTab = LastTab = CurrentTab = newTab();
      nTab = 1;
      Firstbuf = Currentbuf = newbuf;
    } else if (open_new_tab) {
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
  if (line_str) {
    _goLine(line_str);
  }
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
