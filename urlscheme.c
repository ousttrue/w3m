#include "urlscheme.h"

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
