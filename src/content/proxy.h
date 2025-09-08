#pragma once
#include "url.h"
#include "textlist.h"

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

extern char* HTTP_proxy;
extern char* HTTPS_proxy;
extern struct Url HTTP_proxy_parsed;
extern struct Url HTTPS_proxy_parsed;
extern char* NO_proxy;
extern int NOproxy_netaddr;
extern TextList* NO_proxy_domains;
extern char use_proxy;
// #define Do_not_use_proxy (!use_proxy)

struct Url* schemeToProxy(enum UrlScheme scheme);
