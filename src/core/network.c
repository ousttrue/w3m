#include "network.h"
#include "proxy.h"
#include "Str.h"
#include "textlist.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

int DNS_order = DNS_ORDER_UNSPEC;

/* see rc.c, "dns_order" and dnsorders[] */
static int ai_family_order_table[7][3] = {
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 0:unspec */
    { PF_INET, PF_INET6, PF_UNSPEC }, /* 1:inet inet6 */
    { PF_INET6, PF_INET, PF_UNSPEC }, /* 2:inet6 inet */
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 3: --- */
    { PF_INET, PF_UNSPEC, PF_UNSPEC }, /* 4:inet */
    { PF_UNSPEC, PF_UNSPEC, PF_UNSPEC }, /* 5: --- */
    { PF_INET6, PF_UNSPEC, PF_UNSPEC }, /* 6:inet6 */
};

int openSocket(const char* hostname,
    const char* remoteport_name, unsigned short remoteport_num)
{
    if (hostname == 0) {
        goto error;
    }

    volatile int sock = -1;
    struct addrinfo hints, *res0, *res;
    int error;
    // MySignalHandler (*volatile prevtrap)(int _dummy) = NULL;

    /* FIXME: gettextize? */
    // message(getUI(), MSG_INFO, Sprintf("Opening socket...")->ptr);
    // refresh(ttyWriter());

    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     if (sock >= 0)
    //         close(sock);
    //     goto error;
    // }
    // TRAP_ON;

    /* rfc2732 compliance */
    char* hname = Strnew_charp(hostname)->ptr;
    if (hname && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
        hname = allocStr(hostname + 1, -1);
        hname[strlen(hname) - 1] = '\0';
        if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
            goto error;
    }

    int* af;
    for (af = ai_family_order_table[DNS_order];; af++) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = *af;
        hints.ai_socktype = SOCK_STREAM;
        if (remoteport_num != 0) {
            Str portbuf = Sprintf("%d", remoteport_num);
            error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
        } else {
            error = -1;
        }
        if (error && remoteport_name && remoteport_name[0] != '\0') {
            /* try default port */
            error = getaddrinfo(hname, remoteport_name, &hints, &res0);
        }
        if (error) {
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        sock = -1;
        for (res = res0; res; res = res->ai_next) {
            sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0) {
                continue;
            }
            if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                close(sock);
                sock = -1;
                continue;
            }
            break;
        }
        if (sock < 0) {
            freeaddrinfo(res0);
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        freeaddrinfo(res0);
        break;
    }

    // TRAP_OFF;
    return sock;
error:
    // TRAP_OFF;
    return -1;
}

const char* FQDN(const char* host)
{
    if (host == NULL)
        return NULL;

    if (strcasecmp(host, "localhost") == 0)
        return host;

    const char* p;
    for (p = host; *p && *p != '.'; p++)
        ;

    if (*p == '.')
        return host;

    int* af;
    for (af = ai_family_order_table[DNS_order];; af++) {
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char* namebuf;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        hints.ai_family = *af;
        hints.ai_socktype = SOCK_STREAM;
        error = getaddrinfo(host, NULL, &hints, &res0);
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

static int
domain_match(const char* pat, const char* domain)
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

    abort();
}

int check_no_proxy(const char* domain)
{
    TextListItem* tl;
    volatile int ret = 0;
    // MySignalHandler (*volatile prevtrap)(int _dummy) = NULL;

    if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 || domain == NULL)
        return 0;
    for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (domain_match(tl->ptr, domain))
            return 1;
    }
    if (!NOproxy_netaddr) {
        return 0;
    }
    /*
     * to check noproxy by network addr
     */
    // if (sigsetjmp(AbortLoading, 1) != 0) {
    //     ret = 0;
    //     goto end;
    // }
    // TRAP_ON;
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
    // TRAP_OFF;
    return ret;
}
