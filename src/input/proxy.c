#include "proxy.h"
#include "input/isocket.h"
#include "input/url.h"
#include "text/text.h"
#include "trap_jmp.h"
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

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

static int domain_match(char *pat, const char *domain) {
  if (domain == NULL)
    return 0;
  if (*pat == '.')
    pat++;
  for (;;) {
    if (strcasecmp(pat, domain) == 0)
      return 1;
    domain = strchr(domain, '.');
    if (domain == NULL)
      return 0;
    domain++;
  }
}

int check_no_proxy(const char *domain) {

  if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 ||
      domain == NULL) {
    return 0;
  }
  for (auto tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
    if (domain_match(tl->ptr, domain))
      return 1;
  }

  if (!NOproxy_netaddr) {
    return 0;
  }

  /*
   * to check noproxy by network addr
   */
  int ret = 0;
  if (from_jmp()) {
    ret = 0;
    goto end;
  }
  trap_on();
  {
    int error;
    struct addrinfo hints;
    struct addrinfo *res, *res0;
    char addr[4 * 16];

    for (auto af = ai_family_order_table[DNS_order];; af++) {
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = *af;
      error = getaddrinfo(domain, NULL, &hints, &res0);
      if (error) {
        if (*af == PF_UNSPEC) {
          break;
        }
        /* try next */
        continue;
      }
      for (res = res0; res != NULL; res = res->ai_next) {
        switch (res->ai_family) {
        case AF_INET:
          inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                    addr, sizeof(addr));
          break;
        case AF_INET6:
          inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
                    addr, sizeof(addr));
          break;
        default:
          /* unknown */
          continue;
        }
        for (auto tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
          if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
            freeaddrinfo(res0);
            ret = 1;
            goto end;
          }
        }
      }
      freeaddrinfo(res0);
      if (*af == PF_UNSPEC) {
        break;
      }
    }
  }
end:
  trap_off();
  return ret;
}
