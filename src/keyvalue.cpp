#include "keyvalue.h"
#include "app.h"
#include "alloc.h"
#include "Str.h"
#include "quote.h"
#include <string.h>

const char *tag_get_value(struct keyvalue *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return t->value;
  }
  return NULL;
}

int tag_exists(struct keyvalue *t, const char *arg) {
  for (; t; t = t->next) {
    if (!strcasecmp(t->arg, arg))
      return 1;
  }
  return 0;
}

struct keyvalue *cgistr2tagarg(const char *cgistr) {
  keyvalue *t0 = 0;
  keyvalue *t = 0;

  do {
    t = (keyvalue *)New(struct keyvalue);
    t->next = t0;
    t0 = t;
    auto tag = Strnew();
    while (*cgistr && *cgistr != '=' && *cgistr != '&')
      Strcat_char(tag, *cgistr++);
    t->arg = Str_form_unquote(tag)->ptr;
    t->value = NULL;
    if (*cgistr == '\0')
      return t;
    else if (*cgistr == '=') {
      cgistr++;
      auto value = Strnew();
      while (*cgistr && *cgistr != '&')
        Strcat_char(value, *cgistr++);
      t->value = Str_form_unquote(value)->ptr;
    } else if (*cgistr == '&')
      cgistr++;
  } while (*cgistr);

  return t;
}
