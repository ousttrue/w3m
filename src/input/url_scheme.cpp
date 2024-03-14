#include "url_scheme.h"
#include "quote.h"
#include <string.h>
#include <cctype>

struct cmdtable {
  const char *cmdname;
  UrlScheme cmd;
};

struct cmdtable schemetable[] = {
    {"http", SCM_HTTP}, {"local", SCM_LOCAL}, {"file", SCM_LOCAL},
    {"data", SCM_DATA}, {"https", SCM_HTTPS}, {nullptr, SCM_UNKNOWN},
};

std::string schemeNumToName(UrlScheme scheme) {
  for (int i = 0; schemetable[i].cmdname != NULL; i++) {
    if (schemetable[i].cmd == scheme)
      return schemetable[i].cmdname;
  }
  return {};
}

std::optional<UrlScheme> parseUrlScheme(const char **url) {

  const char *p = *url, *q;
  while (*p && (std::isalnum(*p) || *p == '.' || *p == '+' || *p == '-'))
    p++;
  if (*p == ':') { /* scheme found */
    for (auto i = 0; (q = schemetable[i].cmdname) != NULL; i++) {
      int len = strlen(q);
      if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
        *url = p + 1;
        return schemetable[i].cmd;
      }
    }
  }

  return {};
}

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

const char *DefaultFile(UrlScheme scheme) {
  switch (scheme) {
  case SCM_HTTP:
  case SCM_HTTPS:
    return HTTP_DEFAULT_FILE;
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
    return "/";
  default:
    break;
  }
  return NULL;
}
