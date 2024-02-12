#include "app.h"
#include "form.h"
#include "http_response.h"
#include "local_cgi.h"
#include "w3m.h"
#include "downloadlist.h"
#include "tabbuffer.h"
#include "bufferpos.h"
#include "buffer.h"
#include "anchor.h"
#include "myctype.h"
#include "func.h"
#include "alarm.h"
#include "funcname1.h"
#include "signal_util.h"
#include "terms.h"
#include "screen.h"
#include "display.h"
#include "alloc.h"
#include "Str.h"
#include "rc.h"
#include "cookie.h"
#include "history.h"
#include <signal.h>
#include <iostream>

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume and upstream
// PHP does this as well.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

int CurrentKey;
char *CurrentKeyData;
char *CurrentCmdData;
int prec_num = 0;
int prev_key = -1;
bool on_target = true;
#define PREC_LIMIT 10000

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

App::App() {
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

  sync_with_option();
  initCookie();
  if (UseHistory) {
    loadHistory(URLHist);
  }

  TabBuffer::init();
}

App::~App() { std::cout << "App::~App" << std::endl; }

void App::initialize() {}

const char *searchKeyData(void) {
  const char *data = NULL;
  if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
    data = CurrentKeyData;
  else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
    data = CurrentCmdData;
  else if (CurrentKey >= 0)
    data = getKeyData(CurrentKey);
  CurrentKeyData = NULL;
  CurrentCmdData = NULL;
  if (data == NULL || *data == '\0')
    return NULL;
  return allocStr(data, -1);
}

struct Event {
  int cmd;
  void *data;
  Event *next;
};
static Event *CurrentEvent = NULL;
static Event *LastEvent = NULL;
void pushEvent(int cmd, void *data) {
  auto event = (Event *)New(Event);
  event->cmd = cmd;
  event->data = data;
  event->next = NULL;
  if (CurrentEvent)
    LastEvent->next = event;
  else
    CurrentEvent = event;
  LastEvent = event;
}

AlarmEvent DefaultAlarm = {0, AL_UNSET, FUNCNAME_nulcmd, NULL};
static AlarmEvent *CurrentAlarm = &DefaultAlarm;
static void SigAlarm(SIGNAL_ARG) {
  char *data;

  if (CurrentAlarm->sec > 0) {
    CurrentKey = -1;
    CurrentKeyData = NULL;
    CurrentCmdData = data = (char *)CurrentAlarm->data;
    w3mFuncList[CurrentAlarm->cmd].func();
    CurrentCmdData = NULL;
    if (CurrentAlarm->status == AL_IMPLICIT_ONCE) {
      CurrentAlarm->sec = 0;
      CurrentAlarm->status = AL_UNSET;
    }
    if (CurrentTab->currentBuffer()->layout.event) {
      if (CurrentTab->currentBuffer()->layout.event->status != AL_UNSET)
        CurrentAlarm = CurrentTab->currentBuffer()->layout.event;
      else
        CurrentTab->currentBuffer()->layout.event = NULL;
    }
    if (!CurrentTab->currentBuffer()->layout.event)
      CurrentAlarm = &DefaultAlarm;
    if (CurrentAlarm->sec > 0) {
      mySignal(SIGALRM, SigAlarm);
      alarm(CurrentAlarm->sec);
    }
  }
  SIGNAL_RETURN;
}

static bool need_resize_screen = false;
static void resize_screen(void) {
  need_resize_screen = false;
  setlinescols();
  setupscreen(term_entry());
  if (CurrentTab)
    displayBuffer(B_FORCE_REDRAW);
}

void resize_hook(SIGNAL_ARG) {
  need_resize_screen = true;
  mySignal(SIGWINCH, resize_hook);
  SIGNAL_RETURN;
}

static void keyPressEventProc(int c) {
  CurrentKey = c;
  w3mFuncList[(int)GlobalKeymap[c]].func();
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
    Anchor *a;
    Url pu;
    auto s = buf->layout.getCurWord();
    set_environ("W3M_CURRENT_WORD", s.c_str());
    a = retrieveCurrentAnchor(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = retrieveCurrentForm(&buf->layout);
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

int App::mainLoop() {
  displayBuffer(B_FORCE_REDRAW);

  mySignal(SIGWINCH, resize_hook);
  mySignal(SIGCHLD, sig_chld);
  mySignal(SIGPIPE, SigPipe);

  for (;;) {
    if (popAddDownloadList()) {
      ldDL();
    }
    if (CurrentTab->currentBuffer()->layout.submit) {
      Anchor *a = CurrentTab->currentBuffer()->layout.submit;
      CurrentTab->currentBuffer()->layout.submit = NULL;
      CurrentTab->currentBuffer()->layout.gotoLine(a->start.line);
      CurrentTab->currentBuffer()->layout.pos = a->start.pos;
      CurrentTab->_followForm(true);
      continue;
    }

    // event processing
    if (CurrentEvent) {
      CurrentKey = -1;
      CurrentKeyData = NULL;
      CurrentCmdData = (char *)CurrentEvent->data;
      w3mFuncList[CurrentEvent->cmd].func();
      CurrentCmdData = NULL;
      CurrentEvent = CurrentEvent->next;
      continue;
    }

    // get keypress event
    if (CurrentTab->currentBuffer()->layout.event) {
      if (CurrentTab->currentBuffer()->layout.event->status != AL_UNSET) {
        CurrentAlarm = CurrentTab->currentBuffer()->layout.event;
        if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
          CurrentTab->currentBuffer()->layout.event = NULL;
          CurrentKey = -1;
          CurrentKeyData = NULL;
          CurrentCmdData = (char *)CurrentAlarm->data;
          w3mFuncList[CurrentAlarm->cmd].func();
          CurrentCmdData = NULL;
          continue;
        }
      } else
        CurrentTab->currentBuffer()->layout.event = NULL;
    }
    if (!CurrentTab->currentBuffer()->layout.event)
      CurrentAlarm = &DefaultAlarm;
    if (CurrentAlarm->sec > 0) {
      mySignal(SIGALRM, SigAlarm);
      alarm(CurrentAlarm->sec);
    }
    mySignal(SIGWINCH, resize_hook);
    {
      do {
        if (need_resize_screen)
          resize_screen();
      } while (sleep_till_anykey(1, 0) <= 0);
    }
    auto c = getch();
    if (CurrentAlarm->sec > 0) {
      alarm(0);
    }
    if (IS_ASCII(c)) { /* Ascii */
      if (('0' <= c) && (c <= '9') &&
          (prec_num || (GlobalKeymap[(int)c] == FUNCNAME_nulcmd))) {
        prec_num = prec_num * 10 + (int)(c - '0');
        if (prec_num > PREC_LIMIT)
          prec_num = PREC_LIMIT;
      } else {
        set_buffer_environ(CurrentTab->currentBuffer());
        save_buffer_position(&CurrentTab->currentBuffer()->layout);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
  }

  return 0;
}

int searchKeyNum(void) {

  auto d = searchKeyData();
  int n = 1;
  if (d != nullptr)
    n = atoi(d);
  return n * PREC_NUM;
}
