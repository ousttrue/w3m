#include "proxy.h"
#include "setjmp_util.h"
#include "global.h"
#include "textlist.h"
#include "fm.h"
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

ParsedURL HTTP_proxy_parsed;
ParsedURL HTTPS_proxy_parsed;
ParsedURL GOPHER_proxy_parsed;
ParsedURL FTP_proxy_parsed;
TextList* NO_proxy_domains;

void proxyInit()
{
    NO_proxy_domains = newTextList();
}

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

void parse_proxy(void)
{
    if (non_null(HTTP_proxy))
        parseURL(HTTP_proxy, &HTTP_proxy_parsed, NULL);
    if (non_null(HTTPS_proxy))
        parseURL(HTTPS_proxy, &HTTPS_proxy_parsed, NULL);
    if (non_null(GOPHER_proxy))
        parseURL(GOPHER_proxy, &GOPHER_proxy_parsed, NULL);
    if (non_null(FTP_proxy))
        parseURL(FTP_proxy, &FTP_proxy_parsed, NULL);
    if (non_null(NO_proxy))
        set_no_proxy(NO_proxy);
}

static bool is_domain_match(const char* pat, const char* domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;) {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

bool check_no_proxy(const char* domain)
{
    TextListItem* tl;
    volatile int ret = 0;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 || domain == NULL)
        return 0;
    for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (is_domain_match(tl->ptr, domain))
            return 1;
    }
    if (!NOproxy_netaddr) {
        return 0;
    }
    /*
     * to check noproxy by network addr
     */
    if (SETJMP(AbortLoading) != 0) {
        ret = 0;
        goto end;
    }
    TRAP_ON;
    {
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char addr[4 * 16];
        int* af;

        for (af = ai_family_order_table[DNS_order];; af++) {
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
                    inet_ntop(AF_INET,
                        &((struct sockaddr_in*)res->ai_addr)->sin_addr,
                        addr, sizeof(addr));
                    break;
                case AF_INET6:
                    inet_ntop(AF_INET6,
                        &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                    break;
                default:
                    /* unknown */
                    continue;
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
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
    TRAP_OFF;
    return ret;
}
