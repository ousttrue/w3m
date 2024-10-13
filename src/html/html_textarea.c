#include "html/html_textarea.h"
#include "alloc.h"
#include "html/html_parser.h"
#include "html/html_readbuffer.h"
#include "html/html_tag.h"
#include "html/html_text.h"

#define MAX_TEXTAREA                                                           \
  10 /* max number of <textarea>..</textarea>                                  \
      * within one document */

static Str cur_textarea;
Str *textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
int n_textarea;
static int ignore_nl_textarea;
int max_textarea = MAX_TEXTAREA;

void init_textarea() {
  n_textarea = 0;
  cur_textarea = NULL;
  max_textarea = MAX_TEXTAREA;
  textarea_str = New_N(Str, max_textarea);
}

void prerender_textarea() {
  n_textarea = -1;
  if (!max_textarea) { /* halfload */
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
  }
}

struct HtmlTag;
Str process_textarea(struct HtmlTag *tag, int width) {
  Str tmp = NULL;
  char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

  if (cur_form_id < 0) {
    const char *s = "<form_int method=internal action=none>";
    tmp = process_form(parse_tag(&s, true));
  }

  p = "";
  parsedtag_get_value(tag, ATTR_NAME, &p);
  cur_textarea = Strnew_charp(p);
  cur_textarea_size = 20;
  if (parsedtag_get_value(tag, ATTR_COLS, &p)) {
    cur_textarea_size = atoi(p);
    if (strlen(p) > 0 && p[strlen(p) - 1] == '%')
      cur_textarea_size = width * cur_textarea_size / 100 - 2;
    if (cur_textarea_size <= 0) {
      cur_textarea_size = 20;
    } else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX) {
      cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
    }
  }
  cur_textarea_rows = 1;
  if (parsedtag_get_value(tag, ATTR_ROWS, &p)) {
    cur_textarea_rows = atoi(p);
    if (cur_textarea_rows <= 0) {
      cur_textarea_rows = 1;
    } else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX) {
      cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
    }
  }
  cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
  if (n_textarea >= max_textarea) {
    max_textarea *= 2;
    textarea_str = New_Reuse(Str, textarea_str, max_textarea);
  }
  textarea_str[n_textarea] = Strnew();
  ignore_nl_textarea = true;

  return tmp;
}

Str process_n_textarea(void) {
  Str tmp;
  int i;

  if (cur_textarea == NULL)
    return NULL;

  tmp = Strnew();
  Strcat(tmp, Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      cur_hseq, cur_form_id, html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
  if (cur_textarea_readonly)
    Strcat_charp(tmp, " readonly");
  Strcat_charp(tmp, "><u>");
  for (i = 0; i < cur_textarea_size; i++)
    Strcat_char(tmp, ' ');
  Strcat_charp(tmp, "</u></input_alt>]</pre_int>\n");
  cur_hseq++;
  n_textarea++;
  cur_textarea = NULL;

  return tmp;
}

void feed_textarea(const char *str) {
  if (cur_textarea == NULL)
    return;
  if (ignore_nl_textarea) {
    if (*str == '\r')
      str++;
    if (*str == '\n')
      str++;
  }
  ignore_nl_textarea = false;
  while (*str) {
    if (*str == '&')
      Strcat_charp(textarea_str[n_textarea], getescapecmd(&str));
    else if (*str == '\n') {
      Strcat_charp(textarea_str[n_textarea], "\r\n");
      str++;
    } else if (*str == '\r')
      str++;
    else
      Strcat_char(textarea_str[n_textarea], *(str++));
  }
}
