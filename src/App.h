#pragma once
#include <memory>

#include "config.h"
#include <sys/types.h>

class EventDispatcher;
class App {

  App();
  ~App();

public:
  App(const App &) = delete;
  App &operator=(const App &) = delete;
  static App &instance();

  std::shared_ptr<EventDispatcher> dispatcher_;
  void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                       clen_t size);
  void pushEvent(int cmd, void *data);
  void run(class Args &argument);

private:
  void main_loop();
};
