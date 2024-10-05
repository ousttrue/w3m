//
// socket interface
//
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
typedef uintptr_t SocketType;
#else
typedef int SocketType;
#endif

extern int ai_family_order_table[7][3]; /* XXX */

enum DnsOrderMode {
  DNS_ORDER_UNSPEC = 0,
  DNS_ORDER_INET_INET6 = 1,
  DNS_ORDER_INET6_INET = 2,
  DNS_ORDER_INET_ONLY = 4,
  DNS_ORDER_INET6_ONLY = 6,
};
extern enum DnsOrderMode DNS_order;

SocketType socketInvalid();

bool socketOpen(const char *hostname, const char *remoteport_name,
                unsigned short remoteport_num, SocketType *pOut);

int socketWrite(SocketType sock, const char *buf, size_t len);
