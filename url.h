#pragma once

#define SCM_UNKNOWN 255
#define SCM_MISSING 254
#define SCM_HTTP 0
#define SCM_GOPHER 1
#define SCM_FTP 2
#define SCM_FTPDIR 3
#define SCM_LOCAL 4
#define SCM_LOCAL_CGI 5
#define SCM_EXEC 6
#define SCM_NNTP 7
#define SCM_NNTP_GROUP 8
#define SCM_NEWS 9
#define SCM_NEWS_GROUP 10
#define SCM_DATA 11
#define SCM_MAILTO 12
#define SCM_HTTPS 13

struct Str;
struct ParsedURL {
  int scheme;
  char *user;
  char *pass;
  char *host;
  int port;
  char *file;
  char *real_file;
  char *query;
  char *label;
  int is_nocache;
};

#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

void parseURL(char *url, ParsedURL *p_url, ParsedURL *current);
void copyParsedURL(ParsedURL *p, const ParsedURL *q);
void parseURL2(char *url, ParsedURL *pu, ParsedURL *current);
Str *parsedURL2Str(ParsedURL *pu);
Str *parsedURL2RefererStr(ParsedURL *pu);
int getURLScheme(char **url);

extern char *ssl_min_version;
