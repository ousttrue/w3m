#include "message.h"
#include "app.h"
#include "html/readbuffer.h"
#include "html/anchor.h"
#include "display.h"
#include "tabbuffer.h"
#include "buffer.h"
#include "w3m.h"
#include "terms.h"
#include "screen.h"
#include "textlist.h"
#include "http_session.h"

void message(const char *s, int return_x, int return_y) {
  if (!fmInitialized) {
    return;
  }

  move(LASTLINE, 0);
  addnstr(s, COLS - 1);
  clrtoeolx();
  move(return_y, return_x);
}

static GeneralList *message_list = NULL;
static char *delayed_msg = NULL;

void record_err_message(const char *s) {
  if (!fmInitialized) {
    return;
  }
  if (!message_list)
    message_list = newGeneralList();
  if (message_list->nitem >= LINES)
    popValue(message_list);
  pushValue(message_list, allocStr(s, -1));
}

/*
 * List of error messages
 */
std::shared_ptr<Buffer> message_list_panel(void) {
  Str *tmp = Strnew_size(LINES * COLS);
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");
  if (message_list)
    for (auto p = message_list->last; p; p = p->prev)
      Strcat_m_charp(tmp, "<tr><td><pre>", html_quote((char *)p->ptr),
                     "</pre></td></tr>\n", NULL);
  else
    Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
  Strcat_charp(tmp, "</table></body></html>");
  return loadHTMLString(tmp);
}

void disp_err_message(const char *s) {
  record_err_message(s);
  disp_message(s);
}

void disp_message_nsec(const char *s, int sec, int purge) {
  if (IsForkChild)
    return;
  if (!fmInitialized) {
    fprintf(stderr, "%s\n", s);
    return;
  }

  if (CurrentTab != NULL && CurrentTab->currentBuffer() != NULL)
    message(s, CurrentTab->currentBuffer()->layout.AbsCursorX(),
            CurrentTab->currentBuffer()->layout.AbsCursorY());
  else
    message(s, LASTLINE, 0);
  refresh(term_io());
  sleep_till_anykey(sec, purge);
}

void disp_message(const char *s) { disp_message_nsec(s, 10, false); }

void set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }

void refresh_message() {
  if (delayed_msg != NULL) {
    disp_message(delayed_msg);
    delayed_msg = NULL;
    refresh(term_io());
  }
}

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length) {
  int i, j, rate, duration, eta, pos;
  static time_t last_time, start_time;
  time_t cur_time;
  Str *messages;
  char *fmtrbyte, *fmrate;

  if (!fmInitialized)
    return;

  if (*linelen < 1024)
    return;
  if (current_content_length > 0) {
    double ratio;
    cur_time = time(0);
    if (*trbyte == 0) {
      move(LASTLINE, 0);
      clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    move(LASTLINE, 0);
    ratio = 100.0 * (*trbyte) / current_content_length;
    fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
    duration = cur_time - start_time;
    if (duration) {
      rate = *trbyte / duration;
      fmrate = convert_size(rate, 1);
      eta = rate ? (current_content_length - *trbyte) / rate : -1;
      messages = Sprintf("%11s %3.0f%% "
                         "%7s/s "
                         "eta %02d:%02d:%02d     ",
                         fmtrbyte, ratio, fmrate, eta / (60 * 60),
                         (eta / 60) % 60, eta % 60);
    } else {
      messages =
          Sprintf("%11s %3.0f%%                          ", fmtrbyte, ratio);
    }
    addstr(messages->ptr);
    pos = 42;
    i = pos + (COLS - pos - 1) * (*trbyte) / current_content_length;
    move(LASTLINE, pos);
    standout();
    addch(' ');
    for (j = pos + 1; j <= i; j++)
      addch('|');
    standend();
    /* no_clrtoeol(); */
    refresh(term_io());
  } else {
    cur_time = time(0);
    if (*trbyte == 0) {
      move(LASTLINE, 0);
      clrtoeolx();
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    move(LASTLINE, 0);
    fmtrbyte = convert_size(*trbyte, 1);
    duration = cur_time - start_time;
    if (duration) {
      fmrate = convert_size(*trbyte / duration, 1);
      messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
    } else {
      messages = Sprintf("%7s loaded", fmtrbyte);
    }
    message(messages->ptr, 0, 0);
    refresh(term_io());
  }
}
