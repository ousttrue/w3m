#pragma once
#include <assert.h>
#include <string>

// https://datatracker.ietf.org/doc/html/rfc3986#section-3.1
enum UrlScheme {
  SCM_UNKNOWN,
  SCM_HTTP,
  SCM_LOCAL,
  SCM_LOCAL_CGI,
  SCM_DATA,
  SCM_HTTPS,
  SCM_MISSING = 254,
};
inline const char *scheme_str[] = {
    "", "http", "file", "file", "data", "https",
};

inline int getDefaultPort(UrlScheme scheme) {
  if (scheme == SCM_UNKNOWN) {
    return 0;
  }
  if (scheme == SCM_MISSING) {
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
  return DefaultPort[scheme];
}
const char *DefaultFile(UrlScheme scheme);
UrlScheme parseUrlScheme(const char **url);
std::string schemeNumToName(UrlScheme scheme);
