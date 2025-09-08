#include "proxy.h"
#include <stdbool.h>

char* HTTP_proxy = (0);
char* HTTPS_proxy = (0);
struct Url HTTP_proxy_parsed;
struct Url HTTPS_proxy_parsed;
char* NO_proxy = (0);
int NOproxy_netaddr = (true);
TextList* NO_proxy_domains;
char use_proxy = (true);

struct Url*
schemeToProxy(enum UrlScheme scheme)
{
    switch (scheme) {
    case SCM_HTTP:
        return &HTTP_proxy_parsed;
    case SCM_HTTPS:
        return &HTTPS_proxy_parsed;
    default:
#ifdef DEBUG
        abort();
#endif
        return 0;
    }
}
