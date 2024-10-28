#pragma once
#include "text/textlist.h"

extern bool use_proxy;
extern const char *HTTP_proxy;
extern const char *HTTPS_proxy;
extern const char *FTP_proxy;
extern struct TextList *NO_proxy_domains;
extern struct Url HTTP_proxy_parsed;
extern struct Url HTTPS_proxy_parsed;
extern struct Url FTP_proxy_parsed;
extern const char *NO_proxy;
extern bool NOproxy_netaddr;

void parse_proxy();
void set_no_proxy(const char *domains);
