#pragma once

enum UrlScheme {
    SCM_UNKNOWN = 255,
    SCM_MISSING = 254,
    SCM_HTTP = 0,
    SCM_FTP = 2,
    SCM_FTPDIR = 3,
    SCM_LOCAL = 4,
    SCM_LOCAL_CGI = 5,
    SCM_HTTPS = 13,
};

int getDefaultPort(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
const char* schemeNumToName(enum UrlScheme scheme);
