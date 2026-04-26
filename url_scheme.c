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
    // { "gopher", SCM_GOPHER },
    // { "ftp", SCM_FTP },
    { "local", SCM_LOCAL },
    { "file", SCM_LOCAL },
    /*  {"exec", SCM_EXEC}, */
    // { "nntp", SCM_NNTP },
    /*  {"nntp", SCM_NNTP_GROUP}, */
    // { "news", SCM_NEWS },
    /*  {"news", SCM_NEWS_GROUP}, */
    // { "data", SCM_DATA },
    // { "mailto", SCM_MAILTO },
    { "https", SCM_HTTPS },
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
    "file",
    "file",
    // "mailto",
    "https",
};
const char* schemeToStr(enum UrlScheme scheme)
{

    return scheme_str[scheme];
}

/* XXX: note html.h SCM_ */
int DefaultPort[] = {
    80, /* http */
    0, /* local - not defined */
    0, /* local-CGI - not defined? */
    0, /* mailto - not defined */
    443, /* https */
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
    enum UrlScheme scheme = SCM_MISSING;

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
