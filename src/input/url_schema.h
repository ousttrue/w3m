#pragma once
#include <assert.h>

enum UrlSchema {
  SCM_UNKNOWN,
  SCM_HTTP,
  SCM_LOCAL,
  SCM_LOCAL_CGI,
  SCM_DATA,
  SCM_HTTPS,
  SCM_MISSING = 254,
};
inline const char *schema_str[] = {
    "", "http", "file", "file", "data", "https",
};

inline int getDefaultPort(UrlSchema schema) {
  if (schema == SCM_UNKNOWN) {
    return 0;
  }
  if (schema == SCM_MISSING) {
    assert(false);
    return -1;
  }
  static int DefaultPort[] = {
      0,   /* unknown */
      80,  /* http */
      70,  /* gopher */
      21,  /* ftp */
      21,  /* ftpdir */
      0,   /* local - not defined */
      0,   /* local-CGI - not defined? */
      0,   /* exec - not defined? */
      119, /* nntp */
      119, /* nntp group */
      119, /* news */
      119, /* news group */
      0,   /* data - not defined */
      0,   /* mailto - not defined */
      443, /* https */
  };
  return DefaultPort[schema];
}
const char *DefaultFile(UrlSchema schema);
UrlSchema parseUrlSchema(const char **url);
const char *schemaNumToName(UrlSchema schema);
