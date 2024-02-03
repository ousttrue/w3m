#include "url_schema.h"
#include <string.h>
#include <cctype>

struct cmdtable {
  const char *cmdname;
  UrlSchema cmd;
};

struct cmdtable schematable[] = {
    {"http", SCM_HTTP},
    {"gopher", SCM_GOPHER},
    {"ftp", SCM_FTP},
    {"local", SCM_LOCAL},
    {"file", SCM_LOCAL},
    /*  {"exec", SCM_EXEC}, */
    {"nntp", SCM_NNTP},
    /*  {"nntp", SCM_NNTP_GROUP}, */
    {"news", SCM_NEWS},
    /*  {"news", SCM_NEWS_GROUP}, */
    {"data", SCM_DATA},
    {"mailto", SCM_MAILTO},
    {"https", SCM_HTTPS},
    {nullptr, SCM_UNKNOWN},
};

const char *schemaNumToName(UrlSchema schema) {
  int i;

  for (i = 0; schematable[i].cmdname != NULL; i++) {
    if (schematable[i].cmd == schema)
      return schematable[i].cmdname;
  }
  return NULL;
}

UrlSchema parseUrlSchema(const char **url) {
  auto schema = SCM_MISSING;

  const char *p = *url, *q;
  while (*p && (std::isalnum(*p) || *p == '.' || *p == '+' || *p == '-'))
    p++;
  if (*p == ':') { /* schema found */
    schema = SCM_UNKNOWN;
    for (auto i = 0; (q = schematable[i].cmdname) != NULL; i++) {
      int len = strlen(q);
      if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
        schema = schematable[i].cmd;
        *url = p + 1;
        break;
      }
    }
  }
  return schema;
}

/* #define HTTP_DEFAULT_FILE    "/index.html" */

#ifndef HTTP_DEFAULT_FILE
#define HTTP_DEFAULT_FILE "/"
#endif /* not HTTP_DEFAULT_FILE */

const char *DefaultFile(UrlSchema schema) {
  switch (schema) {
  case SCM_HTTP:
  case SCM_HTTPS:
    return HTTP_DEFAULT_FILE;
  case SCM_LOCAL:
  case SCM_LOCAL_CGI:
  case SCM_FTP:
  case SCM_FTPDIR:
    return "/";
  default:
    break;
  }
  return NULL;
}
