#pragma once
#include "libwc/ces.h"
#include "Str.h"

extern const char* HostName;

/*
 * Those SCM_ define the indeces of DefaultPort in url.c and scheme_str[] in
 * _parsedURL2Str in url.c
 */
enum UrlScheme {
    SCM_MISSING = 0,
    SCM_HTTP,
    SCM_GOPHER,
    SCM_FTP,
    SCM_FTPDIR,
    SCM_LOCAL,
    SCM_LOCAL_CGI,
    SCM_EXEC,
    SCM_NNTP,
    SCM_NNTP_GROUP,
    SCM_NEWS,
    SCM_NEWS_GROUP,
    SCM_DATA,
    SCM_MAILTO,
    SCM_GOPHERS,
    SCM_HTTPS,
};

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

const char* schemeNumToName(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
int getDefaultPort(enum UrlScheme scheme);
bool is_localhost(const char* host);
struct Url copyParsedURL(const struct Url* q);
pStr url_quote(const char* str);
pStr url_quote_conv(const char* x, wc_ces c);
struct Url parseURL(const char* src, const struct Url* current);
struct Url parseURL2(const char* src, const struct Url* current);
pStr file_quote(const char* str);
pStr file_unquote(const char* str);
pStr Str_url_unquote(pStr x, bool is_form, bool safe);
static inline pStr Str_form_unquote(pStr x)
{
    return Str_url_unquote(x, true, false);
}
pStr file_to_url(const char* file, const char* CurrentDir);
pStr url_unquote_conv(const char* url, wc_ces charset);
