#pragma once
#include <gcstr/Str.h>
#include <wc.h>
#include "textlist.h"

extern const char* ssl_min_version;
extern int ssl_verify_server;
extern int ssl_path_modified;
extern char* ssl_cert_file;

extern TextList* NO_proxy_domains;

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

extern int DefaultPort[];

struct Url {
    int scheme;
    char* user;
    char* pass;
    char* host;
    int port;
    char* file;
    char* real_file;
    char* query;
    char* label;
    int is_nocache;
};

void parseURL(char* url, struct Url* p_url, struct Url* current);
void copyParsedURL(struct Url* p, const struct Url* q);
void parseURL2(char* url, struct Url* pu, struct Url* current);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);
Str _parsedURL2Str(struct Url* pu, int pass, int user, int label);
char* url_decode2(const char* url, wc_ces url_charset);
char* url_unquote_conv(char* url, wc_ces charset);
char* schemeNumToName(int scheme);
int getURLScheme(char** url);
char* filename_extension(char* patch, int is_url);
struct Url* schemeToProxy(int scheme);
