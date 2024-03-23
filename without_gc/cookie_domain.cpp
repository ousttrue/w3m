#include "cookie_domain.h"
#include "quote.h"
#include "dns_order.h"
#include <regex>

#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

std::string cookie_reject_domains;
std::string cookie_accept_domains;
std::string cookie_avoid_wrong_number_of_dots;
std::list<std::string> Cookie_reject_domains;
std::list<std::string> Cookie_accept_domains;
std::list<std::string> Cookie_avoid_wrong_number_of_dots_domains;

int DNS_order = DNS_ORDER_UNSPEC;

int ai_family_order_table[7][3] = {
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 0:unspec */
    {PF_INET, PF_INET6, PF_UNSPEC},    /* 1:inet inet6 */
    {PF_INET6, PF_INET, PF_UNSPEC},    /* 2:inet6 inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 3: --- */
    {PF_INET, PF_UNSPEC, PF_UNSPEC},   /* 4:inet */
    {PF_UNSPEC, PF_UNSPEC, PF_UNSPEC}, /* 5: --- */
    {PF_INET6, PF_UNSPEC, PF_UNSPEC},  /* 6:inet6 */
};

std::string FQDN(std::string_view host) {
  if (host.empty()) {
    return {};
  }

  if (strcasecmp(host, "localhost") == 0) {
    return {host.begin(), host.end()};
  }

  auto p = host.data();
  auto end = p + host.size();
  for (; p != end && *p != '.'; p++)
    ;

  if (*p == '.') {
    return {host.begin(), host.end()};
  }

  int *af;
  for (af = ai_family_order_table[DNS_order];; af++) {
    int error;
    struct addrinfo hints;
    struct addrinfo *res, *res0;
    char *namebuf;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = *af;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(std::string(host.begin(), host.end()).c_str(), NULL,
                        &hints, &res0);
    if (error) {
      if (*af == PF_UNSPEC) {
        /* all done */
        break;
      }
      /* try next address family */
      continue;
    }
    for (res = res0; res != NULL; res = res->ai_next) {
      if (res->ai_canonname) {
        /* found */
        namebuf = strdup(res->ai_canonname);
        freeaddrinfo(res0);
        return namebuf;
      }
    }
    freeaddrinfo(res0);
    if (*af == PF_UNSPEC) {
      break;
    }
  }
  /* all failed */
  return NULL;
}

std::optional<std::string_view> domain_match(std::string_view host,
                                             std::string_view _domain) {
  // std::string host(_host.begin(), _host.end());
  std::string domain(_domain.begin(), _domain.end());

  /* [RFC 2109] s. 2, "domain-match", case 1
   * (both are IP and identical)
   */
  std::regex re(R"([0-9]+.[0-9]+.[0-9]+.[0-9]+)");
  auto m0 = std::regex_match(std::string(host.begin(), host.end()), re);
  auto m1 = std::regex_match(domain, re);
  // regexCompile("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+", 0);
  // auto m0 = regexMatch(host.data(), host.size(), 1);
  // auto m1 = regexMatch(domain.c_str(), -1, 1);
  if (m0 && m1) {
    if (strcasecmp(host, domain.c_str()) == 0)
      return host;
  } else if (!m0 && !m1) {
    int offset;
    /*
     * "." match all domains (w3m only),
     * and ".local" match local domains ([DRAFT 12] s. 2)
     */
    if (strcasecmp(domain.c_str(), ".") == 0 ||
        strcasecmp(domain.c_str(), ".local") == 0) {
      offset = host.size();
      auto domain_p = &host[offset];
      if (domain[1] == '\0' || contain_no_dots({host.data(), domain_p}))
        return host.substr(offset);
    }
    /*
     * special case for domainName = .hostName
     * see nsCookieService.cpp in Firefox.
     */
    else if (domain[0] == '.' && strcasecmp(host, &domain[1]) == 0) {
      return host;
    }
    /* [RFC 2109] s. 2, cases 2, 3 */
    else {
      offset = (domain[0] != '.') ? 0 : host.size() - domain.size();
      auto domain_p = &host[offset];
      if (offset >= 0 && strcasecmp(domain_p, domain.c_str()) == 0)
        return host.substr(offset);
    }
  }
  return {};
}
