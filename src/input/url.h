#pragma once
#include "text/Str.h"

extern const char *HostName;
extern bool DecodeURL;

enum URL_SCHEME_TYPE {
  SCM_UNKNOWN = 255,
  SCM_MISSING = 254,
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
};

const char *schemeNumToName(enum URL_SCHEME_TYPE scheme);

struct Url {
  enum URL_SCHEME_TYPE scheme;
  char *user;
  char *pass;
  char *host;
  int port;
  const char *file;
  const char *real_file;
  const char *query;
  char *label;
  int is_nocache;
};
#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

const char *file_to_url(const char *file);
void parseURL2(const char *url, struct Url *pu, struct Url *current);

const char *url_decode0(const char *url);
void parseURL(const char *url, struct Url *p_url, struct Url *current);
void copyParsedURL(struct Url *p, const struct Url *q);
Str parsedURL2Str(struct Url *pu);
Str parsedURL2RefererStr(struct Url *pu);
int getURLScheme(const char **url);
extern int is_localhost(const char *host);
extern const char *url_unquote_conv0(const char *url);
extern const char *url_quote(const char *str);
extern Str Str_url_unquote(Str x, bool is_form, bool safe);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
const char *file_quote(const char *str);
const char *file_unquote(const char *str);
