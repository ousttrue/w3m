#pragma once
#include <string>

extern int CurrentKey;
extern int prev_key;
extern char *CurrentKeyData;
extern char *CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;
extern void pushEvent(int cmd, void *data);

void resize_hook(int);
const char *searchKeyData();
int searchKeyNum(void);

class App {
  std::string _currentDir;
  int _currentPid = -1;
  std::string _hostName = "localhost";

  App();

public:
  ~App();
  App(const App &) = delete;
  App &operator=(const App &) = delete;
  static App &instance() {
    static App s_instance;
    return s_instance;
  }

  bool initialize();
  int mainLoop();

  int pid() const { return _currentPid; }
  std::string pwd() const { return _currentDir; }
  std::string hostname() const { return _hostName; }
  bool is_localhost(std::string_view host) const {
    if (host.empty()) {
      return true;
    }
    if (host == "localhost") {
      return true;
    }
    if (host == "127.0.0.1") {
      return true;
    }
    if (host == "[::1]") {
      return true;
    }
    if (host == _hostName) {
      return true;
    }
    return false;
  }
};
