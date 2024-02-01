#pragma once
#include <assert.h>

enum UrlSchema {
  SCM_HTTP = 0,
  SCM_GOPHER = 1,
  SCM_FTP = 2,
  SCM_FTPDIR = 3,
  SCM_LOCAL = 4,
  SCM_LOCAL_CGI = 5,
  SCM_EXEC = 6,
  SCM_NNTP = 7,
  SCM_NNTP_GROUP = 8,
  SCM_NEWS = 9,
  SCM_NEWS_GROUP = 10,
  SCM_DATA = 11,
  SCM_MAILTO = 12,
  SCM_HTTPS = 13,
  SCM_UNKNOWN = 255,
  SCM_MISSING = 254,
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
const char *DefaultFile(UrlSchema schema) ;
UrlSchema parseUrlSchema(const char **url);
const char *schemaNumToName(UrlSchema schema);

