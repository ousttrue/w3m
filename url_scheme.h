#pragma once

enum UrlScheme {
    SCM_HTTP,
    SCM_HTTPS,
    SCM_FILE,
    SCM_LOCAL_CGI,
    SCM_UNKNOWN = 255,
};
const char* schemeToName(enum UrlScheme scheme);
const char* schemeToStr(enum UrlScheme scheme);
enum UrlScheme getURLScheme(const char** url);
int getDefaultPort(enum UrlScheme scheme);
