#include "app.h"
#include "downloadlist.h"
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
    if (Currentbuf->event) {
      if (Currentbuf->event->status != AL_UNSET)
        CurrentAlarm = Currentbuf->event;
      else
        Currentbuf->event = NULL;
    }
    if (!Currentbuf->event)
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

void mainLoop() {
  for (;;) {
    if (popAddDownloadList()) {
      ldDL();
    }
    if (Currentbuf->submit) {
      Anchor *a = Currentbuf->submit;
      Currentbuf->submit = NULL;
      gotoLine(Currentbuf, a->start.line);
      Currentbuf->pos = a->start.pos;
      _followForm(true);
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
