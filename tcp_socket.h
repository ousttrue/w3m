#pragma once
#include <stdint.h>

const char* FQDN(const char* host);
int check_no_proxy(const char* domain);

int tcp_open_v4(const char* hostname, const char* remoteport_name, uint16_t remoteport_num);

int tcp_open_v6(const char* hostname, const char* remoteport_name, uint16_t remoteport_num);

static inline int tcp_open(const char* hostname, const char* remoteport_name, uint16_t remoteport_num)
{
    return tcp_open_v4(hostname, remoteport_name, remoteport_num);
}
