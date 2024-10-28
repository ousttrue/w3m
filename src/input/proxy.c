#include "proxy.h"
#include "input/url.h"
#include "text/text.h"

bool use_proxy = true;
const char *HTTP_proxy = nullptr;
const char *HTTPS_proxy = nullptr;
const char *FTP_proxy = nullptr;
struct Url HTTP_proxy_parsed;
struct Url HTTPS_proxy_parsed;
struct Url FTP_proxy_parsed;
const char *NO_proxy = nullptr;
bool NOproxy_netaddr = true;

void parse_proxy() {
  if (non_null(HTTP_proxy))
    parseURL(HTTP_proxy, &HTTP_proxy_parsed, NULL);
  if (non_null(HTTPS_proxy))
    parseURL(HTTPS_proxy, &HTTPS_proxy_parsed, NULL);
  if (non_null(FTP_proxy))
    parseURL(FTP_proxy, &FTP_proxy_parsed, NULL);
  if (non_null(NO_proxy))
    set_no_proxy(NO_proxy);
}

void set_no_proxy(const char *domains) {
  (NO_proxy_domains = make_domain_list(domains));
}
