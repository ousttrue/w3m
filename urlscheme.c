#include "urlscheme.h"
#include "myctype.h"
#include <string.h>

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
