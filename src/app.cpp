#include "app.h"
#include "screen.h"
#include "ssl_util.h"
#include "html/form.h"
#include "message.h"
#include "http_response.h"
#include "local_cgi.h"
#include "w3m.h"
#include "downloadlist.h"
#include "tabbuffer.h"
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

  _firstTab = _lastTab = _currentTab = new TabBuffer;
  assert(_firstTab);
  _nTab = 1;

  // default dispatcher
  _dispatcher.push([this](const char *buf, int len) {
    this->dispatchPtyIn(buf, len);
    // never exit
    return true;
  });
}

App::~App() {
  if (getpid() != _currentPid) {
    return;
  }

  // clean up only not fork process
  std::cout << "App::~App" << std::endl;
  for (auto &f : _fileToDelete) {
    std::cout << "fileToDelete: " << f << std::endl;
    unlink(f.c_str());
  }
  _fileToDelete.clear();

  stopDownload();
  free_ssl_ctx();
  reset_tty();
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
                    App::instance().dispatch(buf->base, nread);
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

void App::exit(int) {
  // stop libuv
  // exit(i);
  uv_read_stop((uv_stream_t *)&g_tty_in);
  uv_signal_stop(&g_signal_resize);
  uv_timer_stop(&g_timer);
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

  if (_currentTab->currentBuffer()->layout.firstLine == nullptr) {
    return;
  }

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS())
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = nullptr;
  a = (only_img ? nullptr
                : _currentTab->currentBuffer()->layout.retrieveCurrentAnchor());
  if (a == nullptr) {
    a = (only_img ? nullptr
                  : _currentTab->currentBuffer()->layout.retrieveCurrentForm());
    if (a == nullptr) {
      a = _currentTab->currentBuffer()->layout.retrieveCurrentImg();
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    auto pu = urlParse(a->url, _currentTab->currentBuffer()->res->getBaseURL());
    s = Strnew(pu.to_Str());
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  int n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS() - 1))
    offset = (n - 1) * (COLS() - 1);
  disp_message(&s->ptr[offset]);
}

std::string App::currentUrl() const {
  static Str *s = nullptr;
  static int offset = 0;

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS())
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = Strnew(_currentTab->currentBuffer()->res->currentURL.to_Str());
    if (DecodeURL)
      s = Strnew_charp(url_decode0(s->ptr));
  }
  auto n = App::instance().searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS() - 1))
    offset = (n - 1) * (COLS() - 1);

  return &s->ptr[offset];
}

void App::doCmd() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
      invalidate();
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
  invalidate();
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
      set_buffer_environ(_currentTab->currentBuffer());
      _currentTab->currentBuffer()->layout.save_buffer_position();
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

  if (auto a = _currentTab->currentBuffer()->layout.submit) {
    _currentTab->currentBuffer()->layout.submit = NULL;
    _currentTab->currentBuffer()->layout.gotoLine(a->start.line);
    _currentTab->currentBuffer()->layout.pos = a->start.pos;
    if (auto buf = _currentTab->currentBuffer()->followForm(a, true)) {
      pushBuffer(buf, a->target);
    }
  }

  if (_dirty > 0) {
    _dirty = 0;
    _displayBuffer();
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
        Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr, 1, false);
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
  _fileToDelete.push_back(tmpf);
  return tmpf;
}

TabBuffer *App::numTab(int n) const {
  if (n == 0)
    return _currentTab;
  if (n == 1)
    return _firstTab;
  if (_nTab <= 1)
    return nullptr;

  TabBuffer *tab;
  int i;
  for (tab = _firstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void App::drawTabs() {
  if (_nTab > 1) {
    move(0, 0);
    clrtoeolx();
    int y = 0;
    for (auto t = _firstTab; t; t = t->nextTab) {
      y = t->draw(_currentTab);
    }
    move(y + 1, 0);
    for (int i = 0; i < COLS(); i++)
      addch('~');
  }
}

void App::nextTab() {
  if (_nTab <= 1) {
    return;
  }

  for (int i = 0; i < PREC_NUM; i++) {
    if (_currentTab->nextTab)
      _currentTab = _currentTab->nextTab;
    else
      _currentTab = _firstTab;
  }
}

void App::prevTab() {
  if (_nTab <= 1) {
    return;
  }
  for (int i = 0; i < PREC_NUM; i++) {
    if (_currentTab->prevTab)
      _currentTab = _currentTab->prevTab;
    else
      _currentTab = _lastTab;
  }
}

void App::tabRight() {
  TabBuffer *tab;
  int i;
  for (tab = _currentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(_currentTab, tab ? tab : _lastTab, true);
}

void App::tabLeft() {
  TabBuffer *tab;
  int i;
  for (tab = _currentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
    ;
  moveTab(_currentTab, tab ? tab : _firstTab, false);
}

int App::calcTabPos() {
  if (_nTab <= 0) {
    return _lastTab->y;
  }

  TabBuffer *tab;
  int lcol = 0, rcol = 0, col;

  int n2, ny;
  int n1 = (COLS() - rcol - lcol) / TabCols;
  if (n1 >= _nTab) {
    n2 = 1;
    ny = 1;
  } else {
    if (n1 < 0)
      n1 = 0;
    n2 = COLS() / TabCols;
    if (n2 == 0)
      n2 = 1;
    ny = (_nTab - n1 - 1) / n2 + 2;
  }

  // int n2, na, nx, ny, ix, iy;
  int na = n1 + n2 * (ny - 1);
  n1 -= (na - _nTab) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);
  tab = _firstTab;
  for (int iy = 0; iy < ny && tab; iy++) {
    int nx;
    if (iy == 0) {
      nx = n1;
      col = COLS() - rcol - lcol;
    } else {
      nx = n2 - (na - _nTab + (iy - 1)) / (ny - 1);
      col = COLS();
    }
    for (int ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
      tab->x1 = col * ix / nx;
      tab->x2 = col * (ix + 1) / nx - 1;
      tab->y = iy;
      if (iy == 0) {
        tab->x1 += lcol;
        tab->x2 += lcol;
      }
    }
  }
  return _lastTab->y;
}

TabBuffer *App::deleteTab(TabBuffer *tab) {
  if (_nTab <= 1)
    return _firstTab;
  if (tab->prevTab) {
    if (tab->nextTab)
      tab->nextTab->prevTab = tab->prevTab;
    else
      _lastTab = tab->prevTab;
    tab->prevTab->nextTab = tab->nextTab;
    if (tab == _currentTab)
      _currentTab = tab->prevTab;
  } else { /* tab == FirstTab */
    tab->nextTab->prevTab = nullptr;
    _firstTab = tab->nextTab;
    if (tab == _currentTab)
      _currentTab = tab->nextTab;
  }
  _nTab--;
  return _firstTab;
}

void App::moveTab(TabBuffer *t, TabBuffer *t2, int right) {
  if (!t2) {
    t2 = _firstTab;
  }
  if (!t || !t2 || t == t2 || !t) {
    return;
  }
  if (t->prevTab) {
    if (t->nextTab)
      t->nextTab->prevTab = t->prevTab;
    else
      _lastTab = t->prevTab;
    t->prevTab->nextTab = t->nextTab;
  } else {
    t->nextTab->prevTab = nullptr;
    _firstTab = t->nextTab;
  }
  if (right) {
    t->nextTab = t2->nextTab;
    t->prevTab = t2;
    if (t2->nextTab)
      t2->nextTab->prevTab = t;
    else
      _lastTab = t;
    t2->nextTab = t;
  } else {
    t->prevTab = t2->prevTab;
    t->nextTab = t2;
    if (t2->prevTab)
      t2->prevTab->nextTab = t;
    else
      _firstTab = t;
    t2->prevTab = t;
  }
  invalidate();
}

void App::newTab(std::shared_ptr<Buffer> buf) {
  auto tab = new TabBuffer();
  if (!buf) {
    buf = Buffer::create();
    *buf = *_currentTab->currentBuffer();
  }
  tab->firstBuffer = tab->_currentBuffer = buf;

  tab->nextTab = _currentTab->nextTab;
  tab->prevTab = _currentTab;
  if (_currentTab->nextTab)
    _currentTab->nextTab->prevTab = tab;
  else
    _lastTab = tab;
  _currentTab->nextTab = tab;
  _currentTab = tab;
  _nTab++;

  invalidate();
}

void App::pushBuffer(const std::shared_ptr<Buffer> &buf,
                     std::string_view target) {

  if (target.empty() || /* no target specified (that means this page is
  not a
                           frame page) */
      target == "_top"  /* this link is specified to be opened as an
                                   indivisual * page */
  ) {
    CurrentTab->pushBuffer(buf);
  } else {
    Anchor *al = nullptr;
    if (!al) {
      auto label = Strnew_m_charp("_", target, nullptr)->ptr;
      al = CurrentTab->currentBuffer()->layout.name()->searchAnchor(label);
    }
    if (al) {
      CurrentTab->currentBuffer()->layout.gotoLine(al->start.line);
      if (label_topline)
        CurrentTab->currentBuffer()->layout.topLine =
            CurrentTab->currentBuffer()->layout.lineSkip(
                CurrentTab->currentBuffer()->layout.topLine,
                CurrentTab->currentBuffer()->layout.currentLine->linenumber -
                    CurrentTab->currentBuffer()->layout.topLine->linenumber,
                false);
      CurrentTab->currentBuffer()->layout.pos = al->start.pos;
      CurrentTab->currentBuffer()->layout.arrangeCursor();
    }
  }
}
