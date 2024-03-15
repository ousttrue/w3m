#include "keyvalue.h"
#include "url_decode.h"
#include "alloc.h"
#include <sstream>
#include <string.h>

std::string tag_get_value(struct keyvalue *t, const char *arg) {
  for (; t; t = t->next) {
    if (t->arg == arg)
      return t->value;
  }
  return NULL;
}

int tag_exists(struct keyvalue *t, const char *arg) {
  for (; t; t = t->next) {
    if (t->arg == arg)
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
    std::stringstream tag;
    while (*cgistr && *cgistr != '=' && *cgistr != '&')
      tag << *cgistr++;
    t->arg = tag.str();
    t->value = {};
    if (*cgistr == '\0')
      return t;
    else if (*cgistr == '=') {
      cgistr++;
      std::stringstream value;
      while (*cgistr && *cgistr != '&')
        value << *cgistr++;
      t->value = Str_form_unquote(value.str());
    } else if (*cgistr == '&')
      cgistr++;
  } while (*cgistr);

  return t;
}
