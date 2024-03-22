#include "app.h"
// #include "http_request.h"
#include "http_session.h"
#include "tabbuffer.h"
#include "buffer.h"

WSADATA wsaData;
int main(int argc, char **argv) {
  // Initialize Winsock
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
    return 1;
  }

  if (!App::instance().initialize()) {
    return 2;
  }

  if (argc > 1) {
    auto res = loadGeneralFile(argv[1], {}, {.no_referer = true}, {});
    if (!res) {
      return 3;
    }
    auto newbuf = Buffer::create(res);
    CurrentTab->pushBuffer(newbuf);
  }

  return App::instance().mainLoop();
}
