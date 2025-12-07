#pragma once
#include <gcstr/Str.h>
#include <wc/wc.h>
#include "textlist.h"

extern const char* ssl_min_version;
extern int ssl_verify_server;
extern int ssl_path_modified;
extern char* ssl_cert_file;

extern TextList* NO_proxy_domains;

enum UrlScheme {
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

extern int DefaultPort[];

struct Url {
    enum UrlScheme scheme;
    const char* user;
    const char* pass;
    const char* host;
    int port;
    const char* file;
    const char* real_file;
    const char* query;
    const char* label;
    int is_nocache;
};

void parseURL(const char* url, struct Url* p_url, struct Url* current);
void copyParsedURL(struct Url* p, const struct Url* q);
void parseURL2(char* url, struct Url* pu, struct Url* current);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);
Str _parsedURL2Str(struct Url* pu, int pass, int user, int label);
char* url_decode2(const char* url, wc_ces url_charset);
char* url_unquote_conv(const char* url, wc_ces charset);
char* schemeNumToName(int scheme);
enum UrlScheme getURLScheme(const char** url);
char* filename_extension(char* patch, int is_url);
struct Url* schemeToProxy(int scheme);
