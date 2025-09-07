#pragma once

enum DnsOrder {
    DNS_ORDER_UNSPEC = 0,
    DNS_ORDER_INET_INET6 = 1,
    DNS_ORDER_INET6_INET = 2,
    DNS_ORDER_INET_ONLY = 4,
    DNS_ORDER_INET6_ONLY = 6,
};
extern int DNS_order;

int openSocket(const char* hostname, const char* remoteport_name,
    unsigned short remoteport_num);
const char* FQDN(const char* host);
int check_no_proxy(const char* domain);
