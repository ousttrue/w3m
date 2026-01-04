#include "tcp_socket.h"
#include "alloc.h"
#include "textlist.h"
#include "w3m_rc.h"
#include "Str.h"
#include "message.h"
#include "regex.h"
#include "mysignal.h"
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

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

const char* FQDN(const char* host)
{
#ifndef INET6
    struct hostent* entry;
#else /* INET6 */
    int* af;
#endif /* INET6 */

    if (host == NULL)
        return NULL;

    if (strcasecmp(host, "localhost") == 0)
        return host;

    const char* p;
    for (p = host; *p && *p != '.'; p++)
        ;

    if (*p == '.')
        return host;

    for (af = ai_family_order_table[getRuntime()->DNS_order];; af++) {
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

static bool domain_match(const char* pat, const char* domain)
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

static JMP_BUF AbortLoading;

int check_no_proxy(const char* domain)
{
    if (!getRuntime()->NO_proxy_domains) {
        getRuntime()->NO_proxy_domains = newTextList();
    }

    TextListItem* tl;
    volatile int ret = 0;
    PrevTrapFunc prevtrap = NULL;

    if (getRuntime()->NO_proxy_domains == NULL || getRuntime()->NO_proxy_domains->nitem == 0 || domain == NULL)
        return 0;
    for (tl = getRuntime()->NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (domain_match(tl->ptr, domain))
            return 1;
    }
    if (!getRuntime()->NOproxy_netaddr) {
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
#ifndef INET6
        struct hostent* he;
        int n;
        unsigned char** h_addr_list;
        char addr[4 * 16], buf[5];

        he = gethostbyname(domain);
        if (!he) {
            ret = 0;
            goto end;
        }
        for (h_addr_list = (unsigned char**)he->h_addr_list; *h_addr_list;
            h_addr_list++) {
            sprintf(addr, "%d", h_addr_list[0][0]);
            for (n = 1; n < he->h_length; n++) {
                sprintf(buf, ".%d", h_addr_list[0][n]);
                strcat(addr, buf);
            }
            for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
                if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
                    ret = 1;
                    goto end;
                }
            }
        }
#else /* INET6 */
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char addr[4 * 16];
        int* af;

        for (af = ai_family_order_table[getRuntime()->DNS_order];; af++) {
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
                for (tl = getRuntime()->NO_proxy_domains->first; tl != NULL; tl = tl->next) {
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
#endif /* INET6 */
    }
end:
    TRAP_OFF;
    return ret;
}

int tcp_open_v4(const char* hostname,
    const char* remoteport_name, uint16_t remoteport_num)
{
    volatile int sock = -1;
    struct sockaddr_in hostaddr;
    struct hostent* entry;
    struct protoent* proto;
    unsigned short s_port;
    int a1, a2, a3, a4;
    unsigned long adr;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    if (fmInitialized()) {
        /* FIXME: gettextize? */
        message(Sprintf("Opening socket...")->ptr);
    }
    if (SETJMP(AbortLoading) != 0) {
        if (sock >= 0)
            close(sock);
        goto error;
    }
    TRAP_ON;
    if (hostname == NULL) {
        goto error;
    }

    s_port = htons(remoteport_num);
    memset((char*)&hostaddr, 0, sizeof(struct sockaddr_in));
    if ((proto = getprotobyname("tcp")) == NULL) {
        /* protocol number of TCP is 6 */
        proto = New(struct protoent);
        proto->p_proto = 6;
    }
    if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0) {
        goto error;
    }
    regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
    if (regexMatch(hostname, -1, 1)) {
        sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
        adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
        bcopy((void*)&adr, (void*)&hostaddr.sin_addr, sizeof(long));
        hostaddr.sin_family = AF_INET;
        hostaddr.sin_port = s_port;
        if (fmInitialized()) {
            message(Sprintf("Connecting to %s", hostname)->ptr);
        }
        if (connect(sock, (struct sockaddr*)&hostaddr,
                sizeof(struct sockaddr_in))
            < 0) {
            goto error;
        }
    } else {
        char** h_addr_list;
        int result = -1;
        if (fmInitialized()) {
            message(Sprintf("Performing hostname lookup on %s", hostname)->ptr);
        }
        if ((entry = gethostbyname(hostname)) == NULL) {
            goto error;
        }
        hostaddr.sin_family = AF_INET;
        hostaddr.sin_port = s_port;
        for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++) {
            bcopy((void*)h_addr_list[0], (void*)&hostaddr.sin_addr,
                entry->h_length);
            if (fmInitialized()) {
                message(Sprintf("Connecting to %s", hostname)->ptr);
            }
            if ((result = connect(sock, (struct sockaddr*)&hostaddr,
                     sizeof(struct sockaddr_in)))
                == 0) {
                break;
            }
        }
        if (result < 0) {
            goto error;
        }
    }

    TRAP_OFF;
    return sock;
error:
    TRAP_OFF;
    return -1;
}

int tcp_open_v6(const char* hostname,
    const char* remoteport_name, uint16_t remoteport_num)
{
    volatile int sock = -1;
    int* af;
    struct addrinfo hints, *res0, *res;
    int error;
    MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

    if (fmInitialized()) {
        message(Sprintf("Opening socket...")->ptr);
    }
    if (SETJMP(AbortLoading) != 0) {
        if (sock >= 0)
            close(sock);
        goto error;
    }
    TRAP_ON;
    if (hostname == NULL) {
        goto error;
    }

    /* rfc2732 compliance */
    char* hname = Strnew_charp(hostname)->ptr;
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
        hname = allocStr(hostname + 1, -1);
        hname[strlen(hname) - 1] = '\0';
        if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
            goto error;
    }
    for (af = ai_family_order_table[getRuntime()->DNS_order];; af++) {
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

    TRAP_OFF;
    return sock;
error:
    TRAP_OFF;
    return -1;
}
