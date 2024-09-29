#include "isocket.h"
#include "alloc.h"
#include "rand48.h"
#include "terms.h"
#include "fm.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>
#include <setjmp.h>

static JMP_BUF AbortLoading;
static MySignalHandler KeyAbort(SIGNAL_ARG) { LONGJMP(AbortLoading, 1); }

SocketType socketInvalid() { return INVALID_SOCKET; }

bool socketOpen(const char *hostname, const char *remoteport_name,
                unsigned short remoteport_num, SocketType *pOut) {
  SOCKET sock = INVALID_SOCKET;
#ifdef INET6
  int *af;
  struct addrinfo hints, *res0, *res;
  int error;
  char *hname;
#else  /* not INET6 */
  struct sockaddr_in hostaddr;
  struct hostent *entry;
  struct protoent *proto;
  unsigned short s_port;
  int a1, a2, a3, a4;
  unsigned long adr;
#endif /* not INET6 */
  MySignalHandler (*volatile prevtrap)(SIGNAL_ARG) = NULL;

  term_message(Sprintf("Opening socket...")->ptr);
  if (SETJMP(AbortLoading) != 0) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: user abort\n");
#endif
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
    }
    goto error;
  }
  TRAP_ON;
  if (hostname == NULL) {
#ifdef SOCK_DEBUG
    sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n", hostname);
#endif
    goto error;
  }

#ifdef INET6
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
#else /* not INET6 */
  s_port = htons(remoteport_num);
  memset((char *)&hostaddr, 0, sizeof(struct sockaddr_in));
  if ((proto = getprotobyname("tcp")) == NULL) {
    /* protocol number of TCP is 6 */
    proto = New(struct protoent);
    proto->p_proto = 6;
  }
  if ((sock = socket(AF_INET, SOCK_STREAM, proto->p_proto)) < 0) {
#ifdef SOCK_DEBUG
    sock_log("openSocket: socket() failed. reason: %s\n", strerror(errno));
#endif
    goto error;
  }
  regexCompile("^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$", 0);
  if (regexMatch(hostname, -1, 1)) {
    sscanf(hostname, "%d.%d.%d.%d", &a1, &a2, &a3, &a4);
    adr = htonl((a1 << 24) | (a2 << 16) | (a3 << 8) | a4);
    memcpy((void *)&hostaddr.sin_addr, (const void *)&adr, sizeof(long));
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = s_port;
    term_message(Sprintf("Connecting to %s", hostname)->ptr);
    if (connect(sock, (struct sockaddr *)&hostaddr,
                sizeof(struct sockaddr_in)) < 0) {
#ifdef SOCK_DEBUG
      sock_log("openSocket: connect() failed. reason: %s\n", strerror(errno));
#endif
      goto error;
    }
  } else {
    char **h_addr_list;
    int result = -1;
    term_message(Sprintf("Performing hostname lookup on %s", hostname)->ptr);
    if ((entry = gethostbyname(hostname)) == NULL) {
#ifdef SOCK_DEBUG
      sock_log("openSocket: gethostbyname() failed. reason: %s\n",
               strerror(errno));
#endif
      goto error;
    }
    hostaddr.sin_family = AF_INET;
    hostaddr.sin_port = s_port;
    for (h_addr_list = entry->h_addr_list; *h_addr_list; h_addr_list++) {
      memcpy((void *)&hostaddr.sin_addr, (const void *)h_addr_list[0],
             entry->h_length);
#ifdef SOCK_DEBUG
      adr = ntohl(*(long *)&hostaddr.sin_addr);
      sock_log("openSocket: connecting %d.%d.%d.%d\n", (adr >> 24) & 0xff,
               (adr >> 16) & 0xff, (adr >> 8) & 0xff, adr & 0xff);
#endif
      term_message(Sprintf("Connecting to %s", hostname)->ptr);
      if ((result = connect(sock, (struct sockaddr *)&hostaddr,
                            sizeof(struct sockaddr_in))) == 0) {
        break;
      }
#ifdef SOCK_DEBUG
      else {
        sock_log("openSocket: connect() failed. reason: %s\n", strerror(errno));
      }
#endif
    }
    if (result < 0) {
      goto error;
    }
  }
#endif /* not INET6 */

  TRAP_OFF;
  *pOut = sock;
  return true;
error:
  TRAP_OFF;
  return false;
}

int socketWrite(SocketType sock, const char *buf, size_t len) {
  return send(sock, buf, len, 0);
}
