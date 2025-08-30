#include "proxy.h"
#include <stdbool.h>

char* HTTP_proxy = (NULL);
char* HTTPS_proxy = (NULL);
ParsedURL HTTP_proxy_parsed;
ParsedURL HTTPS_proxy_parsed;
char* NO_proxy = (NULL);
int NOproxy_netaddr = (true);
TextList* NO_proxy_domains;
char use_proxy = (true);
