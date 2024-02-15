#pragma once
#include <string>
#include <list>

extern int prev_key;
extern const char *CurrentKeyData;
extern const char *CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;

enum TmpfType {
  TMPF_DFL = 0,
  TMPF_SRC = 1,
  TMPF_FRAME = 2,
  TMPF_CACHE = 3,
  TMPF_COOKIE = 4,
  TMPF_HIST = 5,
  MAX_TMPF_TYPE = 6,
};

class App {
  std::list<std::string> fileToDelete;

  std::string _currentDir;
  int _currentPid = -1;
  std::string _hostName = "localhost";
  std::string _editor = "/usr/bin/vim";

  int _currentKey = -1;

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
  void exit(int rval = 0);

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
  std::string myEditor(const char *file, int line) const;
  const char *searchKeyData();
  int searchKeyNum();
  void _peekURL(bool only_img);
  std::string currentUrl() const;
  void doCmd();
  void doCmd(int cmd, const char *data);
  void dispatchPtyIn(const char *buf, size_t len);
  void onFrame();
  void task(int sec, int cmd, const char *data = nullptr, bool releat = false);
  std::string tmpfname(TmpfType type, const std::string &ext);
};
