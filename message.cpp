#include "message.h"
#include "display.h"
#include "buffer.h"
#include "w3m.h"
#include "terms.h"
#include "screen.h"
#include "indep.h"
#include "textlist.h"
#include "readbuffer.h"

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
Buffer *message_list_panel(void) {
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

void disp_err_message(const char *s, int redraw_current) {
  record_err_message(s);
  disp_message(s, redraw_current);
}

void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
                       int mouse) {
  if (QuietMessage)
    return;
  if (!fmInitialized) {
    fprintf(stderr, "%s\n", s);
    return;
  }

  if (CurrentTab != NULL && Currentbuf != NULL)
    message(s, Currentbuf->cursorX + Currentbuf->rootX,
            Currentbuf->cursorY + Currentbuf->rootY);
  else
    message(s, LASTLINE, 0);
  refresh(term_io());
  sleep_till_anykey(sec, purge);
  if (CurrentTab != NULL && Currentbuf != NULL && redraw_current)
    displayBuffer(Currentbuf, B_NORMAL);
}

void disp_message(const char *s, int redraw_current) {
  disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
}

void set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }

void refresh_message() {
  if (delayed_msg != NULL) {
    disp_message(delayed_msg, FALSE);
    delayed_msg = NULL;
    refresh(term_io());
  }
}
