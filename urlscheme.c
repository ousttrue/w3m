#include "urlscheme.h"
#include "myctype.h"
#include <string.h>

static int DefaultPort[] = {
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

int getDefaultPort(enum UrlScheme scheme)
{
    return DefaultPort[scheme];
}

struct {
    const char* cmdname;
    int cmd;
} schemetable[] = {
    { "http", SCM_HTTP },
    { "ftp", SCM_FTP },
    { "local", SCM_LOCAL },
    { "file", SCM_LOCAL },
    { "https", SCM_HTTPS },
    { 0, SCM_UNKNOWN },
};

enum UrlScheme getURLScheme(const char** url)
{
    const char *p = *url, *q;
    int i;
    int scheme = SCM_MISSING;

    while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
        p++;
    if (*p == ':') { /* scheme found */
        scheme = SCM_UNKNOWN;
        for (i = 0; (q = schemetable[i].cmdname) != 0; i++) {
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

const char* schemeNumToName(enum UrlScheme scheme)
{
    for (int i = 0; schemetable[i].cmdname != NULL; i++) {
        if (schemetable[i].cmd == scheme)
            return schemetable[i].cmdname;
    }
    return NULL;
}
