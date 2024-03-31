#include "cmp.h"
#include <assert.h>
#include <ctype.h>

int strcasecmp(std::string_view l, std::string_view r) {
  auto ll = l.begin();
  auto rr = r.begin();
  for (; ll != l.end() && rr != r.end(); ++ll, ++rr) {
    auto cmp = toupper(*ll) - toupper(*rr);
    if (cmp) {
      return cmp;
    }
  }
  if (ll == l.end() && rr == r.end()) {
    return 0;
  } else if (ll == l.end()) {
    return 1;
  } else if (rr == r.end()) {
    return -1;
  } else {
    assert(false);
    return 0;
  }
}

#if defined(_MSC_VER)
int strncasecmp(const char *l, const char *r, size_t n) {
  std::string_view ll(l);
  if (ll.size() > n) {
    ll = ll.substr(0, n);
  }
  std::string_view rr(r);
  if (rr.size() > n) {
    rr = rr.substr(0, n);
  }
  return strcasecmp(ll, rr);
}

int strcasecmp(const char *l, const char *r) {
  return strcasecmp(std::string_view(l), std::string_view(r));
}
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
char *strcasestr(const char *str, const char *pattern) {
  size_t i;
  unsigned char c0 = *pattern, c1, c2;

  if (c0 == '\0')
    return (char *)str;

  c0 = toupper(c0);
  for (; (c1 = *str) != '\0'; str++) {
    if (toupper(c1) == c0) {
      for (i = 1;; i++) {
        c2 = pattern[i];
        if (c2 != '\0')
          return (char *)str;
        c1 = str[i];
        if (toupper(c1) != toupper(c2))
          break;
      }
    }
  }
  return NULL;
}
#endif
