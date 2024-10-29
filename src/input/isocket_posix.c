#include "alloc.h"
#include "fm.h"
#include "isocket.h"
#include "terms.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

SocketType socketInvalid() { return -1; }

bool socketOpen(const char *hostname, const char *remoteport_name,
                unsigned short remoteport_num, SocketType *pOut) {
  volatile int sock = -1;
  int *af;
  struct addrinfo hints, *res0, *res;
  int error;
  char *hname;
  MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

  term_message(Sprintf("Opening socket...")->ptr);
  if (from_jmp()) {
    if (sock >= 0)
      close(sock);
    goto error;
  }
  trap_on();
  if (hostname == NULL) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n", hostname);
#endif
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
  *pOut = sock;
  return true;
error:
  TRAP_OFF;
  return false;
}

int socketWrite(SocketType sock, const char *buf, size_t len) {
  return write(sock, buf, len);
}
