#include "app.h"
#include "form.h"
#include "message.h"
#include "http_response.h"
#include "local_cgi.h"
#include "w3m.h"
#include "downloadlist.h"
#include "tabbuffer.h"
#include "bufferpos.h"
#include "buffer.h"
#include "myctype.h"
#include "func.h"
#include "funcname1.h"
#include "terms.h"
#include "display.h"
#include "alloc.h"
#include "Str.h"
#include "rc.h"
#include "cookie.h"
#include "history.h"
#include <iostream>
#include <sstream>
#include <uv.h>
#include <chrono>
#include <unistd.h>

const auto FRAME_INTERVAL_MS = std::chrono::milliseconds(1000 / 15);

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume and upstream
// PHP does this as well.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

const char *CurrentKeyData;
const char *CurrentCmdData;
int prec_num = 0;
int prev_key = -1;
bool on_target = true;
#define PREC_LIMIT 10000

// static void sig_chld(int signo) {
//   int p_stat;
//   pid_t pid;
//
//   while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
//     DownloadList *d;
//
//     if (WIFEXITED(p_stat)) {
//       for (d = FirstDL; d != nullptr; d = d->next) {
//         if (d->pid == pid) {
//           d->err = WEXITSTATUS(p_stat);
//           break;
//         }
//       }
//     }
//   }
//   mySignal(SIGCHLD, sig_chld);
//   return;
// }

// static void SigPipe(SIGNAL_ARG) {
//   mySignal(SIGPIPE, SigPipe);
//   SIGNAL_RETURN;
// }

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

App::App() {
  static int s_i = 0;
  assert(s_i == 0);
  ++s_i;

  //
  // process
  //
  _currentPid = getpid();

#ifdef MAXPATHLEN
  {
    char buf[MAXPATHLEN];
    getcwd(buf, MAXPATHLEN);
    _currentDir = buf;
  }
#else
  _currentDir = getcwd(nullptr, 0);
#endif

  {
    char hostname[HOST_NAME_MAX + 2];
    if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
      /* Don't use hostname if it is truncated.  */
      hostname[HOST_NAME_MAX + 1] = '\0';
      auto hostname_len = strlen(hostname);
      if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX) {
        _hostName = hostname;
      }
    }
  }

  if (_editor.empty()) {
    if (auto p = getenv("EDITOR")) {
      _editor = p;
    }
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

  FirstTab = LastTab = CurrentTab = new TabBuffer;
  assert(FirstTab);
  nTab = 1;
}

App::~App() {
  if (getpid() != _currentPid) {
    return;
  }

  // clean up only not fork process
  std::cout << "App::~App" << std::endl;
  for (auto &f : fileToDelete) {
    std::cout << "fileToDelete: " << f << std::endl;
    unlink(f.c_str());
  }
  fileToDelete.clear();
}

bool App::initialize() {
  init_rc();

  if (!BookmarkFile) {
    BookmarkFile = rcFile(BOOKMARK);
  }

  sync_with_option();
  initCookie();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();
  if (UseHistory) {
    loadHistory(URLHist);
  }

  return true;
}

static void set_buffer_environ(const std::shared_ptr<Buffer> &buf) {
  static std::shared_ptr<Buffer> prev_buf;
  static Line *prev_line = nullptr;
  static int prev_pos = -1;
  Line *l;

  if (buf == nullptr)
    return;
  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->res->sourcefile.c_str());
    set_environ("W3M_FILENAME", buf->res->filename.c_str());
    set_environ("W3M_TITLE", buf->layout.title.c_str());
    set_environ("W3M_URL", buf->res->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->res->type.size() ? buf->res->type.c_str() : "unknown");
  }
  l = buf->layout.currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->layout.pos != prev_pos)) {
    Url pu;
    auto s = buf->layout.getCurWord();
    set_environ("W3M_CURRENT_WORD", s.c_str());
    auto a = buf->layout.retrieveCurrentAnchor();
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = buf->layout.retrieveCurrentImg();
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = buf->layout.retrieveCurrentForm();
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->layout.currentColumn +
                                                        buf->layout.cursorX + 1)
                                          ->ptr);
  } else if (!l) {
    set_environ("W3M_CURRENT_WORD", "");
    set_environ("W3M_CURRENT_LINK", "");
    set_environ("W3M_CURRENT_IMG", "");
    set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", "0");
    set_environ("W3M_CURRENT_COLUMN", "0");
  }
  prev_buf = buf;
  prev_line = l;
  prev_pos = buf->layout.pos;
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
  *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

uv_tty_t g_tty_in;
uv_signal_t g_signal_resize;
uv_timer_t g_timer;

int App::mainLoop() {
  fmInit();
  displayBuffer();

  //
  // stdin tty read
  //
  uv_tty_init(uv_default_loop(), &g_tty_in, 0, 1);
  uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_RAW);
  uv_read_start((uv_stream_t *)&g_tty_in, &alloc_buffer,
                [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
                  if (nread < 0) {
                    if (nread == UV_EOF) {
                      // end of file
                      uv_close((uv_handle_t *)&g_tty_in, NULL);
                    }
                  } else if (nread > 0) {
                    // process key input
                    App::instance().dispatchPtyIn(buf->base, nread);
                  }

                  // OK to free buffer as write_data copies it.
                  if (buf->base) {
                    free(buf->base);
                  }
                });

  //
  // SIGWINCH
  //
  uv_signal_init(uv_default_loop(), &g_signal_resize);
  uv_signal_start(
      &g_signal_resize, [](uv_signal_t *handle, int signum) { setResize(); },
      SIGWINCH);

  //
  // timer
  //
  uv_timer_init(uv_default_loop(), &g_timer);
  uv_timer_start(
      &g_timer, [](uv_timer_t *handle) { App::instance().onFrame(); }, 0,
      FRAME_INTERVAL_MS.count());

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());
  return 0;
}

int App::searchKeyNum() {
  int n = 1;
  if (auto d = searchKeyData()) {
    n = atoi(d);
  }
  return n * PREC_NUM;
}

std::string App::myEditor(const char *file, int line) const {
  std::stringstream tmp;
  bool set_file = false;
  bool set_line = false;
  for (auto p = _editor.begin(); *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      tmp << file;
      set_file = true;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      tmp << line;
      set_line = true;
      p++;
    } else {
      tmp << *p;
    }
  }
  if (!set_file) {
    if (!set_line && line > 1 && strcasestr(_editor.c_str(), "vi")) {
      tmp << " +" << line;
    }
    tmp << " " << file;
  }
  return tmp.str();
}

const char *App::searchKeyData() {
  const char *data = NULL;
  if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
    data = CurrentKeyData;
  else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
    data = CurrentCmdData;
  else if (_currentKey >= 0)
    data = getKeyData(_currentKey);
  CurrentKeyData = NULL;
  CurrentCmdData = NULL;
  if (data == NULL || *data == '\0')
    return NULL;
  return allocStr(data, -1);
}

void App::_peekURL(bool only_img) {
  Anchor *a;
  static Str *s = nullptr;
  static int offset = 0;

  if (CurrentTab->currentBuffer()->layout.firstLine == nullptr) {
    return;
  }

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = nullptr;
  a = (only_img ? nullptr
                : CurrentTab->currentBuffer()->layout.retrieveCurrentAnchor());
  if (a == nullptr) {
    a = (only_img ? nullptr
                  : CurrentTab->currentBuffer()->layout.retrieveCurrentForm());
    if (a == nullptr) {
      a = CurrentTab->currentBuffer()->layout.retrieveCurrentImg();
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    auto pu = urlParse(a->url, CurrentTab->currentBuffer()->res->getBaseURL());
    s = Strnew(pu.to_Str());
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  int n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  disp_message(&s->ptr[offset], true);
}

std::string App::currentUrl() const {
  static Str *s = nullptr;
  static int offset = 0;

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = Strnew(CurrentTab->currentBuffer()->res->currentURL.to_Str());
    if (DecodeURL)
      s = Strnew_charp(url_decode0(s->ptr));
  }
  auto n = App::instance().searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);

  return &s->ptr[offset];
}

void App::doCmd() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
      displayBuffer();
      return;
    }
  }
  /* data: FUNC [DATA] [; FUNC [DATA] ...] */
  while (*data) {
    SKIP_BLANKS(data);
    if (*data == ';') {
      data++;
      continue;
    }
    auto p = getWord(&data);
    auto cmd = getFuncList(p);
    if (cmd < 0)
      break;
    p = getQWord(&data);
    doCmd(cmd, p);
  }
  displayBuffer();
}

void App::doCmd(int cmd, const char *data) {
  _currentKey = -1;
  CurrentKeyData = nullptr;
  CurrentCmdData = *data ? data : nullptr;
  w3mFuncList[cmd].func();
  CurrentCmdData = nullptr;
}

void App::dispatchPtyIn(const char *buf, size_t len) {
  if (len == 0) {
    return;
  }
  auto c = buf[0];
  if (IS_ASCII(c)) { /* Ascii */
    if (('0' <= c) && (c <= '9') &&
        (prec_num || (GlobalKeymap[(int)c] == FUNCNAME_nulcmd))) {
      prec_num = prec_num * 10 + (int)(c - '0');
      if (prec_num > PREC_LIMIT)
        prec_num = PREC_LIMIT;
    } else {
      set_buffer_environ(CurrentTab->currentBuffer());
      save_buffer_position(&CurrentTab->currentBuffer()->layout);
      _currentKey = c;
      w3mFuncList[(int)GlobalKeymap[(int)c]].func();
      prec_num = 0;
    }
  }
  prev_key = _currentKey;
  _currentKey = -1;
  CurrentKeyData = NULL;
}

void App::onFrame() {
  if (popAddDownloadList()) {
    ldDL();
  }

  if (CurrentTab->currentBuffer()->layout.submit) {
    Anchor *a = CurrentTab->currentBuffer()->layout.submit;
    CurrentTab->currentBuffer()->layout.submit = NULL;
    CurrentTab->currentBuffer()->layout.gotoLine(a->start.line);
    CurrentTab->currentBuffer()->layout.pos = a->start.pos;
    CurrentTab->_followForm(true);
    return;
  }
}

struct TimerTask {
  int cmd;
  std::string data;
  uv_timer_t handle;
};
std::list<std::shared_ptr<TimerTask>> g_timers;

void App::task(int sec, int cmd, const char *data, bool repeat) {
  if (cmd >= 0) {
    auto t = std::make_shared<TimerTask>();
    t->cmd = cmd;
    t->data = data ? data : "";
    t->handle.data = t.get();
    g_timers.push_back(t);

    uv_timer_init(uv_default_loop(), &t->handle);
    if (repeat) {
      uv_timer_start(
          &t->handle,
          [](uv_timer_t *handle) {
            auto _t = (TimerTask *)handle->data;
            App::instance().doCmd(_t->cmd, _t->data.c_str());
          },
          0, sec * 1000);
    } else {
      uv_timer_start(
          &t->handle,
          [](uv_timer_t *handle) {
            auto _t = (TimerTask *)handle->data;
            App::instance().doCmd(_t->cmd, _t->data.c_str());
            for (auto it = g_timers.begin(); it != g_timers.end(); ++it) {
              if (it->get() == _t) {
                g_timers.erase(it);
                break;
              }
            }
          },
          sec * 1000, 0);
    }
    disp_message_nsec(
        Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr, false, 1,
        false, true);
  } else {
    // cancel timer
    g_timers.clear();
  }
}

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

std::string App::tmpfname(TmpfType type, const std::string &ext) {
  std::stringstream ss;
  ss << rc_dir << "/w3m" << tmpf_base[type] << App::instance().pid()
     << tmpf_seq[type]++ << ext;
  auto tmpf = ss.str();
  fileToDelete.push_back(tmpf);
  return tmpf;
}
