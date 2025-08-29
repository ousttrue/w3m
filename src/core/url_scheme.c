#include "url_scheme.h"
#include <myctype.h>
#include <string.h>

/* XXX: note html.h SCM_ */
int DefaultPort[] = {
    80, /* http */
    70, /* gopher */
    21, /* ftp */
    21, /* ftpdir */
    0, /* local - not defined */
    0, /* local-CGI - not defined? */
    0, /* exec - not defined? */
    119, /* nntp */
    119, /* nntp group */
    119, /* news */
    119, /* news group */
    0, /* data - not defined */
    0, /* mailto - not defined */
    443, /* https */
};

struct SchemeTable {
    const char* cmdname;
    enum UrlScheme cmd;
};

struct SchemeTable schemetable[] = {
    { "http", SCM_HTTP },
    { "gopher", SCM_GOPHER },
    { "ftp", SCM_FTP },
    { "local", SCM_LOCAL },
    { "file", SCM_LOCAL },
    /*  {"exec", SCM_EXEC}, */
    { "nntp", SCM_NNTP },
    /*  {"nntp", SCM_NNTP_GROUP}, */
    { "news", SCM_NEWS },
    /*  {"news", SCM_NEWS_GROUP}, */
    { "data", SCM_DATA },
    { "mailto", SCM_MAILTO },
    { "https", SCM_HTTPS },
    { 0, SCM_UNKNOWN },
};

const char*
schemeNumToName(enum UrlScheme scheme)
{
    for (int i = 0; schemetable[i].cmdname; i++) {
        if (schemetable[i].cmd == scheme)
            return schemetable[i].cmdname;
    }
    return 0;
}

enum UrlScheme getURLScheme(char** url)
{
    char* p = *url;
    while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        p++;

    int scheme = SCM_MISSING;
    if (*p == ':') { /* scheme found */
        scheme = SCM_UNKNOWN;
        const char* q;
        for (int i = 0; (q = schemetable[i].cmdname); i++) {
            int len = strlen(q);
            if (!strncasecmp(q, *url, len) && (*url)[len] == ':') {
                scheme = schemetable[i].cmd;
                *url = p + 1;
                break;
            }
        }
    }
    return scheme;
}
