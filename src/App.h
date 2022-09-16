#pragma once
#include "config.h"
#include <sys/types.h>

class App {
  App();
  ~App();

public:
  App(const App &) = delete;
  App &operator=(const App &) = delete;
  static App &instance();

  bool add_download_list = false;
  void addDownloadList(pid_t pid, char *url, char *save, char *lock,
                       clen_t size);
  void run(class Args &argument);

private:
  void main_loop();
};
