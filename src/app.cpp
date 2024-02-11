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
#include <signal.h>

int CurrentKey;
char *CurrentKeyData;
char *CurrentCmdData;
int prec_num = 0;
int prev_key = -1;
bool on_target = true;
#define PREC_LIMIT 10000

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
    if (Currentbuf->layout.event) {
      if (Currentbuf->layout.event->status != AL_UNSET)
        CurrentAlarm = Currentbuf->layout.event;
      else
        Currentbuf->layout.event = NULL;
    }
    if (!Currentbuf->layout.event)
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
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
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
    set_environ("W3M_SOURCEFILE", buf->info->sourcefile.c_str());
    set_environ("W3M_FILENAME", buf->info->filename.c_str());
    set_environ("W3M_TITLE", buf->layout.title.c_str());
    set_environ("W3M_URL", buf->info->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->info->type.size() ? buf->info->type.c_str() : "unknown");
  }
  l = buf->layout.currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->layout.pos != prev_pos)) {
    Anchor *a;
    Url pu;
    char *s = GetWord(buf);
    set_environ("W3M_CURRENT_WORD", s ? s : "");
    a = retrieveCurrentAnchor(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->info->getBaseURL());
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = retrieveCurrentImg(&buf->layout);
    if (a) {
      pu = urlParse(a->url, buf->info->getBaseURL());
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
void mainLoop() {
  for (;;) {
    if (popAddDownloadList()) {
      ldDL();
    }
    if (Currentbuf->layout.submit) {
      Anchor *a = Currentbuf->layout.submit;
      Currentbuf->layout.submit = NULL;
      Currentbuf->layout.gotoLine(a->start.line);
      Currentbuf->layout.pos = a->start.pos;
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
    if (Currentbuf->layout.event) {
      if (Currentbuf->layout.event->status != AL_UNSET) {
        CurrentAlarm = Currentbuf->layout.event;
        if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
          Currentbuf->layout.event = NULL;
          CurrentKey = -1;
          CurrentKeyData = NULL;
          CurrentCmdData = (char *)CurrentAlarm->data;
          w3mFuncList[CurrentAlarm->cmd].func();
          CurrentCmdData = NULL;
          continue;
        }
      } else
        Currentbuf->layout.event = NULL;
    }
    if (!Currentbuf->layout.event)
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
        set_buffer_environ(Currentbuf);
        save_buffer_position(&Currentbuf->layout);
        keyPressEventProc((int)c);
        prec_num = 0;
      }
    }
    prev_key = CurrentKey;
    CurrentKey = -1;
    CurrentKeyData = NULL;
  }
}

int searchKeyNum(void) {

  auto d = searchKeyData();
  int n = 1;
  if (d != nullptr)
    n = atoi(d);
  return n * PREC_NUM;
}
