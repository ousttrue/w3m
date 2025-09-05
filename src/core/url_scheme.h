#pragma once

enum UrlScheme {
    SCM_UNKNOWN = 0,
    SCM_HTTP = 1,
    SCM_GOPHER = 2,
    SCM_FTP = 3,
    SCM_FTPDIR = 4,
    SCM_LOCAL = 5,
    SCM_LOCAL_CGI = 6,
    SCM_EXEC = 7,
    SCM_NNTP = 8,
    SCM_NNTP_GROUP = 9,
    SCM_NEWS = 10,
    SCM_NEWS_GROUP = 11,
    SCM_DATA = 12,
    SCM_MAILTO = 13,
    SCM_HTTPS = 14,
    SCM_MISSING = 255,
};

struct SchemeInfo {
    const char* name;
    enum UrlScheme scheme;
    int port;
};
struct SchemeInfo getSchemeInfo(enum UrlScheme scheme);

enum UrlScheme parseUrlScheme(const char** url);
inline static enum UrlScheme getUrlScheme(const char* url)
{
    const char* tmp = url;
    return parseUrlScheme(&tmp);
}
