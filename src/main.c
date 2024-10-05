#include "alloc.h"
#include "buffer/buffer.h"
#include "buffer/display.h"
#include "buffer/downloadlist.h"
#include "buffer/tabbuffer.h"
#include "core.h"
#include "defun.h"
#include "fm.h"
#include "history.h"
#include "html/html_readbuffer.h"
#include "input/http_cookie.h"
#include "input/http_request.h"
#include "input/loader.h"
#include "input/localcgi.h"
#include "input/url_stream.h"
#include "rc.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "term/tty.h"
#include "text/ctrlcode.h"
#include "text/text.h"
#include <fcntl.h>
#ifdef _WIN32
#else
#include <sys/wait.h>
#endif
#include <gc.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

struct TabBuffer *CurrentTab = nullptr;
struct TabBuffer *FirstTab = nullptr;
struct TabBuffer *LastTab = nullptr;

void show_params(FILE *fp);
static int show_params_p = 0;

#define GC_WARN_KEEP_MAX (20)
static GC_warn_proc orig_GC_warn_proc = NULL;

static void wrap_GC_warn_proc(const char *msg, GC_word arg) {

  /* *INDENT-OFF* */
  static struct {
    const char *msg;
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
      tty_sleep_till_anykey(1, 1);
    }

    lock = 0;
  }

  // else if (orig_GC_warn_proc)
  //   orig_GC_warn_proc(msg, arg);
  // else
  //   fprintf(stderr, msg, (unsigned long)arg);
}

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
}

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
  /* FIXME: gettextize? */
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
  fprintf(f, "    -ppc count       specify the number of pixels per character "
             "(4.0...32.0)\n");
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

#ifdef _WIN32
#else
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
    if (WIFEXITED(p_stat)) {
      download_exit(pid, WEXITSTATUS(p_stat));
    }
  }
  mySignal(SIGCHLD, sig_chld);
  return;
}

static MySignalHandler SigPipe(SIGNAL_ARG) { mySignal(SIGPIPE, SigPipe); }
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <locale.h>
#include <windows.h>
#include <winsock2.h>
WSADATA wsaData;
#endif

int main(int argc, char **argv) {
  if (argc < 2) {
    return 1;
  }

#ifdef _WIN32
  /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
  WORD wVersionRequested = MAKEWORD(2, 2);

  int err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    /* Tell the user that we could not find a usable */
    /* Winsock DLL.                                  */
    printf("WSAStartup failed with error: %d\n", err);
    return 1;
  }

  // utf-8
  // https://learn.microsoft.com/ja-jp/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-170#utf-8-support
  // setlocale(LC_ALL, "ja_JP.Utf-8");
  // setlocale(LC_ALL, ".931");
  setlocale(LC_ALL, ".UTF-8");
  // setlocale(LC_ALL, "");
  // SetConsoleOutputCP(65001);
  // setvbuf(stdout, nullptr, _IOFBF, 1000);
  // _setmode(_fileno(stdout), _O_U8TEXT);;

#endif

  if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL")) {
    set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
  }
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif

  // struct Buffer *newbuf = NULL;
  // char *p;
  // int c, i;
  // InputStream redin;
  // int load_bookmark = false;
  // int visual_start = false;
  // int open_new_tab = false;
  // char search_header = false;
  // char *default_type = NULL;
  // char *post_file = NULL;
  // Str err_msg;

  url_stream_init();
  initialize();

  // char **load_argv = New_N(char *, argc - 1);
  // int load_argc = 0;

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

  /* initializations */
  init_rc();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();

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

  FirstTab = NULL;
  LastTab = NULL;
  nTab = 0;
  CurrentTab = NULL;
  CurrentKey = -1;
  if (BookmarkFile == NULL)
    BookmarkFile = rcFile(BOOKMARK);

  term_fmInit();
#ifndef _WIN32
  mySignal(SIGWINCH, resize_hook);
#endif

  sync_with_option();
  initCookie();
  if (UseHistory)
    loadHistory(URLHist);

#ifndef _WIN32
  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);
#endif

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif
  auto err_msg = Strnew();

  // if (load_argc == 0) {
  //   /* no URL specified */
  //   if (load_bookmark) {
  //     newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
  //     if (newbuf == NULL)
  //       Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
  //   } else if (visual_start) {
  //     /* FIXME: gettextize? */
  //     Str s_page;
  //     s_page =
  //         Strnew_charp("<title>W3M startup page</title><center><b>Welcome to
  //         ");
  //     Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
  //     Strcat_m_charp(
  //         s_page, "w3m</a>!<p><p>This is w3m version ", w3m_version,
  //         "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori
  //         Ito</a>", NULL);
  //     newbuf = loadHTMLString(s_page);
  //     if (newbuf == NULL)
  //       Strcat_charp(err_msg, "w3m: Can't load string.\n");
  //     else if (newbuf != NO_BUFFER)
  //       newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
  //   } else if ((p = getenv("HTTP_HOME")) != NULL ||
  //              (p = getenv("WWW_HOME")) != NULL) {
  //     newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
  //     if (newbuf == NULL)
  //       Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
  //     else if (newbuf != NO_BUFFER)
  //       pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
  //   } else {
  //     term_fmTerm();
  //     usage();
  //   }
  //   if (newbuf == NULL) {
  //     term_fmTerm();
  //     if (err_msg->length)
  //       fprintf(stderr, "%s", err_msg->ptr);
  //     w3m_exit(2);
  //   }
  //   i = -1;
  // }
  // else {
  //   i = 0;
  // }

  // for (; i < load_argc; i++) {
  //   if (i >= 0) {
  // SearchHeader = search_header;
  // DefaultType = default_type;

  const char *url = argv[1];
  if (getURLScheme(&url) == SCM_MISSING && !ArgvIsURL)
    url = file_to_url(url);
  else
    url = url_quote(argv[1]);

  // if (post_file && i == 0) {
  //   FILE *fp;
  //   Str body;
  //   if (!strcmp(post_file, "-"))
  //     fp = stdin;
  //   else
  //     fp = fopen(post_file, "r");
  //   if (fp == NULL) {
  //     /* FIXME: gettextize? */
  //     Strcat(err_msg, Sprintf("w3m: Can't open %s.\n", post_file));
  //     return 1;
  //   }
  //   body = Strfgetall(fp);
  //   if (fp != stdin)
  //     fclose(fp);
  //   request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
  //   request->body = body->ptr;
  //   request->boundary = NULL;
  //   request->length = body->length;
  // } else {
  //   request = NULL;
  // }

  struct FormList *request = nullptr;
  auto newbuf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);

  if (newbuf == NULL) {
    /* FIXME: gettextize? */
    Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", url));
    return 1;
  } else if (newbuf == NO_BUFFER) {
    return 1;
  }

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

  if (newbuf == NO_BUFFER)
    return 1;

  // if ((newbuf->real_scheme == SCM_LOCAL && newbuf->header_source &&
  //      newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
  //   newbuf->search_header = search_header;

  if (CurrentTab == NULL) {
    FirstTab = LastTab = CurrentTab = newTab();
    nTab = 1;
    Firstbuf = Currentbuf = newbuf;
  }
  // else if (open_new_tab) {
  //   _newT();
  //   Currentbuf->nextBuffer = newbuf;
  //   delBuffer(Currentbuf);
  // }
  else {
    Currentbuf->nextBuffer = newbuf;
    Currentbuf = newbuf;
  }
  {
    Currentbuf = newbuf;
    saveBufferInfo();
  }

  // if (do_add_download_list()) {
  //   CurrentTab = LastTab;
  //   if (!FirstTab) {
  //     FirstTab = LastTab = CurrentTab = newTab();
  //     nTab = 1;
  //   }
  //   if (!Firstbuf || Firstbuf == NO_BUFFER) {
  //     Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
  //     Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
  //     Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
  //   } else
  //     Currentbuf = Firstbuf;
  //   ldDL();
  // } else
  { CurrentTab = FirstTab; }

  if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
    if (newbuf == NO_BUFFER) {
      term_input("Hit any key to quit w3m:");
    }
    term_fmTerm();
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
    disp_message_nsec(err_msg->ptr, false, 1, true, false);

  DefaultType = NULL;

  Currentbuf = Firstbuf;
  displayBuffer(Currentbuf, B_FORCE_REDRAW);

  char *line_str = NULL;
  mainloop(line_str);
}
