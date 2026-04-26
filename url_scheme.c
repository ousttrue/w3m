#include "url_scheme.h"
#include "myctype.h"
#include <string.h>
#include <strings.h>

struct NameMap {
    const char* name;
    enum UrlScheme scheme;
};

struct NameMap schemetable[] = {
    { "http", SCM_HTTP },
    { "https", SCM_HTTPS },
    { "file", SCM_FILE },
    { "cgi", SCM_LOCAL_CGI },
    { 0, SCM_UNKNOWN },
};

const char* schemeToName(enum UrlScheme scheme)
{
    for (int i = 0; schemetable[i].name; i++) {
        if (schemetable[i].scheme == scheme)
            return schemetable[i].name;
    }
    return 0;
}

static const char* scheme_str[] = {
    "http",
    "https",
    "file",
    "cgi",
};

const char* schemeToStr(enum UrlScheme scheme)
{
    return scheme_str[scheme];
}

int DefaultPort[] = {
    80, /* http */
    443, /* https */
    0, /* file - not defined */
    0, /* local-CGI - not defined */
};

int getDefaultPort(enum UrlScheme scheme)
{
    return scheme != SCM_UNKNOWN
        ? DefaultPort[scheme]
        : 0;
}

enum UrlScheme getURLScheme(const char** url)
{
    const char *p = *url, *q;
    enum UrlScheme scheme = SCM_UNKNOWN;

    while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        p++;
    if (*p == ':') { /* scheme found */
        scheme = SCM_UNKNOWN;
        for (int i = 0; (q = schemetable[i].name); i++) {
            int len = strlen(q);
            if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
                scheme = schemetable[i].scheme;
                *url = p + 1;
                break;
            }
        }
    }
    return scheme;
}
