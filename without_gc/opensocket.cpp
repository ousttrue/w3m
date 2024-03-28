#include "opensocket.h"
#include "cookie_domain.h"
#include <string.h>
#include <sstream>

#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

std::optional<int> openSocket(const std::string &hostname,
                              const std::string &remoteport_name,
                              unsigned short remoteport_num) {
  // App::instance().message(Sprintf("Opening socket...")->ptr);
  //   if (SETJMP(AbortLoading) != 0) {
  // #ifdef SOCK_DEBUG
  //     sock_log("openSocket() failed. reason: user abort\n");
  // #endif
  //     if (sock >= 0)
  //       close(sock);
  //     goto error;
  //   }
  //   TRAP_ON;
  if (hostname.empty()) {
    // #ifdef SOCK_DEBUG
    //     sock_log("openSocket() failed. reason: Bad hostname \"%s\"\n",
    //     hostname);
    // #endif
    return {};
  }

  /* rfc2732 compliance */
  auto hname = hostname;
  if (hname[0] == '[' && hname.back() == ']') {
    hname = hname.substr(1, hname.size() - 2);
    if (strspn(hname.c_str(), "0123456789abcdefABCDEF:.") != hname.size()) {
      return {};
    }
  }

  int sock = -1;
  for (auto af = ai_family_order_table[DNS_order];; af++) {
    struct addrinfo hints = {0};
    // memset(&hints, 0, sizeof(hints));
    hints.ai_family = *af;
    hints.ai_socktype = SOCK_STREAM;

    int error = -1;
    struct addrinfo *res0;
    if (remoteport_num) {
      std::stringstream portbuf;
      portbuf << remoteport_num;
      error = getaddrinfo(hname.c_str(), portbuf.str().c_str(), &hints, &res0);
    }
    if (error && remoteport_name.size() && remoteport_name[0] != '\0') {
      /* try default port */
      error =
          getaddrinfo(hname.c_str(), remoteport_name.c_str(), &hints, &res0);
    }
    if (error) {
      if (*af == PF_UNSPEC) {
        return -1;
      }
      /* try next ai family */
      continue;
    }

    for (auto res = res0; res; res = res->ai_next) {
      sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
      if (sock < 0) {
        continue;
      }
      if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
#ifdef _MSC_VER
        closesocket(sock);
#else
        close(sock);
#endif
        sock = -1;
        continue;
      }
      break;
    }
    if (sock < 0) {
      freeaddrinfo(res0);
      if (*af == PF_UNSPEC) {
        return {};
      }
      /* try next ai family */
      continue;
    }
    freeaddrinfo(res0);
    break;
  }

  return sock;
}
