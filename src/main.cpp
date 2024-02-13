#include "app.h"
#include "http_request.h"
#include "http_session.h"
#include "tabbuffer.h"
#include "buffer.h"

int main(int argc, char **argv) {
  if (!App::instance().initialize()) {
    return 1;
  }

  if (argc > 1) {
    auto res = loadGeneralFile(argv[1], {}, {.referer = NO_REFERER}, {});
    if (!res) {
      return 2;
    }
    auto newbuf = Buffer::create(res);
    CurrentTab->pushBuffer(newbuf);
  }

  return App::instance().mainLoop();
}
