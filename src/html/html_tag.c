#include "html/html_tag.h"
#include "alloc.h"
#include "input/url.h"
#include "text/Str.h"

const char *tag_get_value(struct HtmlTag *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return t->value;
  }
  return NULL;
}

bool tag_exists(struct HtmlTag *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return 1;
  }
  return 0;
}

struct HtmlTag *cgistr2tagarg(const char *cgistr) {
  Str tag;
  Str value;
  struct HtmlTag *t0, *t;

  t = t0 = NULL;
  do {
    t = New(struct HtmlTag);
    t->next = t0;
    t0 = t;
    tag = Strnew();
    while (*cgistr && *cgistr != '=' && *cgistr != '&')
      Strcat_char(tag, *cgistr++);
    t->arg = Str_form_unquote(tag)->ptr;
    t->value = NULL;
    if (*cgistr == '\0')
      return t;
    else if (*cgistr == '=') {
      cgistr++;
      value = Strnew();
      while (*cgistr && *cgistr != '&')
        Strcat_char(value, *cgistr++);
      t->value = Str_form_unquote(value)->ptr;
    } else if (*cgistr == '&')
      cgistr++;
  } while (*cgistr);
  return t;
}
