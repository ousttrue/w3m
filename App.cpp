#include "App.h"
#include "core.h"
extern "C" {
#include "fm.h"
#include "proto.h"
}

const auto PREC_LIMIT = 10000;

App::App() {}

App::~App() {}

App &App::instance() {
  static App s_instance;
  return s_instance;
}

void App::addDownloadList(pid_t pid, char *url, char *save, char *lock,
                          clen_t size) {

  auto d = New(DownloadList);
  d->pid = pid;
  d->url = url;
  if (save[0] != '/' && save[0] != '~')
    save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
  d->save = expandPath(save);
  d->lock = lock;
  d->size = size;
  d->time = time(0);
  d->running = TRUE;
  d->err = 0;
  d->next = NULL;
  d->prev = LastDL;
  if (LastDL)
    LastDL->next = d;
  else
    FirstDL = d;
  LastDL = d;
  add_download_list = TRUE;
}

void App::main_loop() {
  for (;;) {
    if (add_download_list) {
      add_download_list = false;
      ldDL();
    }
    if (Currentbuf->submit) {
      Anchor *a = Currentbuf->submit;
      Currentbuf->submit = nullptr;
      gotoLine(Currentbuf, a->start.line);
      Currentbuf->pos = a->start.pos;
      _followForm(TRUE);
      continue;
    }
    /* event processing */
    if (CurrentEvent) {
      CurrentKey = -1;
      CurrentKeyData = nullptr;
      CurrentCmdData = (char *)CurrentEvent->data;
      w3mFuncList[CurrentEvent->cmd].func();
      CurrentCmdData = nullptr;
      CurrentEvent = CurrentEvent->next;
      continue;
    }
    /* get keypress event */
    if (Currentbuf->event) {
      if (Currentbuf->event->status != AL_UNSET) {
        CurrentAlarm = Currentbuf->event;
        if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
          Currentbuf->event = nullptr;
          CurrentKey = -1;
          CurrentKeyData = nullptr;
          CurrentCmdData = (char *)CurrentAlarm->data;
          w3mFuncList[CurrentAlarm->cmd].func();
          CurrentCmdData = nullptr;
          continue;
        }
      } else
        Currentbuf->event = nullptr;
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
    CurrentKeyData = nullptr;
  }
}
