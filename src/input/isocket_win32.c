#include "input/isocket.h"
#include "trap_jmp.h"
#include "alloc.h"
#include "rand48.h"
#include "term/terms.h"
#include "fm.h"
#include <winsock2.h>
#include <ws2tcpip.h>

SocketType socketInvalid() { return INVALID_SOCKET; }

bool socketOpen(const char *hostname, const char *remoteport_name,
                unsigned short remoteport_num, SocketType *pOut) {
  SOCKET sock = INVALID_SOCKET;
  int *af;
  struct addrinfo hints, *res0, *res;
  int error;
  char *hname;

  term_message(Sprintf("Opening socket...")->ptr);
  if (from_jmp()) {
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
    }
    goto error;
  }
  trap_on();
  if (hostname == NULL) {
    goto error;
  }

  /* rfc2732 compliance */
  hname = allocStr(hostname, -1);
  if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
    hname = allocStr(hostname + 1, -1);
    hname[strlen(hname) - 1] = '\0';
    if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
      goto error;
  }
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
    sock = INVALID_SOCKET;
    for (res = res0; res; res = res->ai_next) {
      sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sock == INVALID_SOCKET) {
        continue;
      }
      if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        continue;
      }
      break;
    }
    if (sock == INVALID_SOCKET) {
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

  trap_off();
  *pOut = sock;
  return true;
error:
  trap_off();
  return false;
}

int socketWrite(SocketType sock, const char *buf, size_t len) {
  return send(sock, buf, len, 0);
}
