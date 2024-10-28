#include "message.h"
#include "alloc.h"
#include "html/html_text.h"
#include "term/termsize.h"
#include "text/Str.h"
#include "text/textlist.h"

static struct GeneralList *message_list = NULL;

// void term_err_message(const char *s) {
void message_push(const char *s) {
  if (!message_list)
    message_list = newGeneralList();
  if (message_list->nitem >= LINES)
    popValue(message_list);
  pushValue(message_list, allocStr(s, -1));
}

static const char *term_message_to_html() {
  if (!message_list) {
    return "<tr><td>(no message recorded)</td></tr>\n";
  }

  auto tmp = Strnew();
  for (auto p = message_list->last; p; p = p->prev)
    Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
                   "</pre></td></tr>\n", NULL);
  return tmp->ptr;
}

const char *message_list_panel(int cols) {
  Str tmp = Strnew_size(LINES * COLS);
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");
  Strcat_m_charp(tmp, term_message_to_html());
  Strcat_charp(tmp, "</table></body></html>");
  return tmp->ptr;
}

// void disp_err_message(const char *s, int redraw_current);
// void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
//                        int mouse);
// void disp_message(const char *s, int redraw_current);
// #define disp_message_nomouse disp_message
// void set_delayed_message(const char *s);
// void term_show_delayed_message();

// void disp_err_message(const char *s, int redraw_current) {
//   term_err_message(s);
//   disp_message(s, redraw_current);
// }

// void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
//                        int mouse) {
//   if (QuietMessage)
//     return;
//
//   if (term_is_initialized()) {
//     if (CurrentTab != NULL && Currentbuf != NULL) {
//       scr_message(s,
//                   Currentbuf->document->viewport.cursorX +
//                       Currentbuf->document->viewport.rootX,
//                   Currentbuf->document->viewport.cursorY +
//                       Currentbuf->document->viewport.rootY);
//     } else {
//       scr_message(s, LASTLINE, 0);
//     }
//     term_refresh();
//
//     // nsec
//     tty_sleep_till_anykey(sec, purge);
//     if (CurrentTab != NULL && Currentbuf != NULL && redraw_current) {
//       displayBuffer(Currentbuf, B_NORMAL);
//     }
//   } else {
//     fprintf(stderr, "%s\n", s);
//   }
// }
//
// void disp_message(const char *s, int redraw_current) {
//   disp_message_nsec(s, redraw_current, 10, false, true);
// }
//
// static char *delayed_msg = NULL;
// void set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }
//
// void term_show_delayed_message() {
//   if (delayed_msg != NULL) {
//     disp_message(delayed_msg, false);
//     delayed_msg = NULL;
//     term_refresh();
//   }
// }
