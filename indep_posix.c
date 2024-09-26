#include "Str.h"
#include "myctype.h"
#include "indep.h"
#include <pwd.h>

char *expandPath(char *name) {
  char *p;
  struct passwd *passent, *getpwnam(const char *);
  Str extpath = NULL;

  if (name == NULL)
    return NULL;
  p = name;
  if (*p == '~') {
    p++;
    if (IS_ALPHA(*p)) {
      char *q = strchr(p, '/');
      if (q) { /* ~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* ~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent)
        goto rest;
      extpath = Strnew_charp(passent->pw_dir);
    } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
      extpath = Strnew_charp(getenv("HOME"));
    } else
      goto rest;
    if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
      p++;
    Strcat_charp(extpath, p);
    return extpath->ptr;
  }
rest:
  return name;
}
