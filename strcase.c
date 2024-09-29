#include "strcase.h"
#include "myctype.h"
#include <string.h>

#ifdef _WIN32
/* string search using the simplest algorithm */
char *strcasestr(const char *s1, const char *s2) {
  if (s2 == nullptr)
    return (char *)s1;
  if (*s2 == '\0')
    return (char *)s1;
  int len1 = strlen(s1);
  int len2 = strlen(s2);
  while (*s1 && len1 >= len2) {
    if (strncasecmp(s1, s2, len2) == 0)
      return (char *)s1;
    s1++;
    len1--;
  }
  return 0;
}
#else
int strcasecmp(const char *s1, const char *s2) {
  int x;
  while (*s1) {
    x = TOLOWER(*s1) - TOLOWER(*s2);
    if (x != 0)
      return x;
    s1++;
    s2++;
  }
  return -TOLOWER(*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  int x;
  while (*s1 && n) {
    x = TOLOWER(*s1) - TOLOWER(*s2);
    if (x != 0)
      return x;
    s1++;
    s2++;
    n--;
  }
  return n ? -TOLOWER(*s2) : 0;
}
#endif /* not HAVE_STRCASECMP */

static int strcasematch(char *s1, char *s2) {
  int x;
  while (*s1) {
    if (*s2 == '\0')
      return 1;
    x = TOLOWER(*s1) - TOLOWER(*s2);
    if (x != 0)
      break;
    s1++;
    s2++;
  }
  return (*s2 == '\0');
}

/* search multiple strings */
int strcasemstr(char *str, char *srch[], char **ret_ptr) {
  int i;
  while (*str) {
    for (i = 0; srch[i]; i++) {
      if (strcasematch(str, srch[i])) {
        if (ret_ptr)
          *ret_ptr = str;
        return i;
      }
    }
    str++;
  }
  return -1;
}
