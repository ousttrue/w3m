#pragma once

enum UrlScheme {
    SCM_UNKNOWN = 255,
    SCM_MISSING = 254,
    SCM_HTTP = 0,
    SCM_HTTPS = 1,
    SCM_FTP = 2,
    SCM_FTPDIR = 3,
    SCM_LOCAL = 4,
    SCM_LOCAL_CGI = 5,
};
inline static int getDefaultPort(enum UrlScheme scheme)
{
    switch (scheme) {
    case SCM_HTTP:
        return 80;
    case SCM_HTTPS:
        return 443;
    case SCM_FTP:
    case SCM_FTPDIR:
        return 21;
    default:
        return 0;
    }
}

int getDefaultPort(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
const char* schemeNumToName(enum UrlScheme scheme);
