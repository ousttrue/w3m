#include "url_scheme.h"
#include <myctype.h>
#include <string.h>
#include <strings.h>

struct SchemeInfo schemetable[] = {
    { 0, SCM_UNKNOWN, 0 },
    { "http", SCM_HTTP, 80 },
    { "gopher", SCM_GOPHER, 70 },
    { "ftp", SCM_FTP, 21 },
    { "ftp", SCM_FTPDIR, 21 },
    { "file", SCM_LOCAL, 0 },
    { "file", SCM_LOCAL_CGI, 0 },
    { "exec", SCM_EXEC, 0 },
    { "nntp", SCM_NNTP, 119 },
    { "nntp", SCM_NNTP_GROUP, 119 },
    { "news", SCM_NEWS, 119 },
    { "news", SCM_NEWS_GROUP, 119 },
    { "data", SCM_DATA, 0 },
    { "mailto", SCM_MAILTO, 0 },
    { "https", SCM_HTTPS, 443 },
};

struct SchemeInfo getSchemeInfo(enum UrlScheme scheme)
{
    for (int i = 0; i < sizeof(schemetable) / sizeof(schemetable[0]); ++i) {
        if (schemetable[i].scheme == scheme) {
            return schemetable[i];
        }
    }
    return (struct SchemeInfo) {
        0,
        SCM_MISSING,
        0,
    };
}

enum UrlScheme parseUrlScheme(const char** url)
{
    if (url && *url) {
        const char* p = *url;
        while (*p && (IS_ALNUM(*p) || *p == '.' || *p == '+' || *p == '-'))
            p++;
        if (*p == ':') { /* scheme found */
            for (int i = 1; i < sizeof(schemetable) / sizeof(schemetable[0]); ++i) {
                const char* q = schemetable[i].name;
                int len = p-*url;
                if (strncasecmp(q, *url, len) == 0) {
                    *url = p + 1;
                    return schemetable[i].scheme;
                }
            }

            // ???:
            *url = p + 1;
            return SCM_UNKNOWN;
        } else {
            // no ':'
            return SCM_MISSING;
        }
    }
    // empty string as SCM_UNKNOWN
    return SCM_UNKNOWN;
}
