#pragma once

enum UrlScheme {
    SCM_HTTP,
    SCM_GOPHER,
    SCM_LOCAL,
    SCM_LOCAL_CGI,
    SCM_EXEC,
    SCM_DATA,
    SCM_MAILTO,
    SCM_HTTPS,
    SCM_UNKNOWN = 255,
    SCM_MISSING = 254,
};
const char* schemeToName(enum UrlScheme scheme);
const char* schemeToStr(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
int getDefaultPort(enum UrlScheme scheme);
