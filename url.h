#pragma once

/*
 * Those SCM_ define the indeces of DefaultPort in url.c and scheme_str[] in
 * _parsedURL2Str in url.c
 */
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
    SCM_GOPHERS = 13,
    SCM_HTTPS = 14,
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
typedef struct Url ParsedURL;

const char* schemeNumToName(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
