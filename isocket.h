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

SocketType socketInvalid();

bool socketOpen(const char *hostname, const char *remoteport_name,
                unsigned short remoteport_num, SocketType *pOut);

int socketWrite(SocketType sock, const char *buf, size_t len);
