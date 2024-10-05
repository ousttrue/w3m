#include "file/shell.h"
#include "text/Str.h"
#include "text/myctype.h"

#define is_shell_unsafe(c) (GET_QUOTE_TYPE(c) & SHELL_UNSAFE_MASK)

const char *shell_quote(const char *str) {
  Str tmp = NULL;
  for (auto p = str; *p; p++) {
    if (is_shell_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '\\');
      Strcat_char(tmp, *p);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}
