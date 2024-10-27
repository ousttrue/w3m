#include "message.h"
#include "term/terms.h"
#include "term/termsize.h"
#include "text/Str.h"

const char *message_list_panel(int cols) {
  Str tmp = Strnew_size(LINES * COLS);
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");
  Strcat_m_charp(tmp, term_message_to_html());
  Strcat_charp(tmp, "</table></body></html>");
  return tmp->ptr;
}
