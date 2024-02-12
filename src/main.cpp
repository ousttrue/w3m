#include "w3m.h"
#include "app.h"
#include "textlist.h"
#include "tabbuffer.h"
#include "http_request.h"
#include "http_session.h"
#include "buffer.h"
#include "cookie.h"
#include "history.h"
#include "local_cgi.h"
#include "signal_util.h"
#include "display.h"
#include "terms.h"
#include "myctype.h"
#include "rc.h"
#include "downloadlist.h"
#include <unistd.h>
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

static void SigPipe(SIGNAL_ARG) {
  mySignal(SIGPIPE, SigPipe);
  SIGNAL_RETURN;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return 1;
  }

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

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif

  fileToDelete = newTextList();
  CurrentDir = currentdir();
  CurrentPid = (int)getpid();
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

  const char *p;
  if (!non_null(Editor) && (p = getenv("EDITOR")) != nullptr)
    Editor = p;
  if (!non_null(Mailer) && (p = getenv("MAILER")) != nullptr)
    Mailer = p;

  FirstTab = nullptr;
  LastTab = nullptr;
  nTab = 0;
  CurrentTab = nullptr;
  CurrentKey = -1;
  if (!BookmarkFile) {
    BookmarkFile = rcFile(BOOKMARK);
  }

  fmInit();
  mySignal(SIGWINCH, resize_hook);

  sync_with_option();
  initCookie();
  if (UseHistory) {
    loadHistory(URLHist);
  }

  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);

  auto res = loadGeneralFile(argv[1], {}, {.referer = NO_REFERER}, {});
  if (!res) {
    return 2;
  }

  auto newbuf = Buffer::create(res);

  TabBuffer::init();
  CurrentTab->pushBuffer(newbuf);

  displayBuffer(B_FORCE_REDRAW);

  return App::instance().mainLoop();
}
