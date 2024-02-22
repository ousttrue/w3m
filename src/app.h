#pragma once
#include "rowcol.h"
#include <string>
#include <list>
#include <memory>
#include <functional>
#include <stack>
#include <vector>
#include <array>
#include <unordered_map>

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

using Func = std::function<void()>;

using Dispatcher = std::function<bool(const char *buf, size_t len)>;

struct TabBuffer;
struct Buffer;
struct Screen;
class App {
  RowCol _size = {};

  std::shared_ptr<Screen> _screen;
  bool _fmInitialized = false;

  int _dirty = 1;
  std::list<std::string> _fileToDelete;

  std::string _currentDir;
  int _currentPid = -1;
  std::string _hostName = "localhost";
  std::string _editor = "/usr/bin/vim";

  std::unordered_map<std::string, Func> w3mFuncList;
  char _currentKey = -1;
  char prev_key = -1;

  TabBuffer *_firstTab = 0;
  TabBuffer *_lastTab = 0;
  int _nTab = 0;
  TabBuffer *_currentTab = 0;

  std::stack<Dispatcher> _dispatcher;

  std::array<std::string, 128> GlobalKeymap;
  std::unordered_map<std::string, std::string> keyData;

  App();
  void onResize();

public:
  ~App();
  App(const App &) = delete;
  App &operator=(const App &) = delete;
  static App &instance() {
    static App s_instance;
    return s_instance;
  }

  int LINES() const { return _size.row; }
  int COLS() const { return _size.col; }
  int LASTLINE() const { return (_size.row - 1); }
  int INIT_BUFFER_WIDTH() { return App::instance().COLS() - (1); }

  std::shared_ptr<Screen> screen() const { return _screen; }
  bool initialize();
  void beginRawMode();
  void endRawMode();
  bool isRawMode() const { return _fmInitialized; }
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
  // FuncId getFuncList(const std::string &id) const {
  //   auto found = _funcTable.find(id);
  //   if (found != _funcTable.end()) {
  //     return found->second;
  //   }
  //   return (FuncId)-1;
  // }
  void doCmd();
  void doCmd(const std::string &cmd, const char *data);
  void dispatchPtyIn(const char *buf, size_t len);
  void dispatch(const char *buf, size_t len) {
    if (!_dispatcher.top()(buf, len)) {
      _dispatcher.pop();
    }
  }

  void initKeymap(bool force);
  void setKeymap(const char *p, int lineno, int verbose);
  const char *getKeyData(char key) {
    auto func = GlobalKeymap[key];
    auto found = keyData.find(func);
    if (found == keyData.end()) {
      return {};
    }
    return found->second.c_str();
  }

  void pushDispatcher(const Dispatcher &dispatcher) {
    _dispatcher.push(dispatcher);
  }
  void onFrame();
  void task(int sec, const std::string &cmd, const char *data = nullptr,
            bool releat = false);
  std::string tmpfname(TmpfType type, const std::string &ext);
  void invalidate() { ++_dirty; }

  // tabs
  TabBuffer *FirstTab() const { return _firstTab; }
  TabBuffer *LastTab() const { return _lastTab; }
  int nTab() const { return _nTab; }
  TabBuffer *CurrentTab() const { return _currentTab; }
  void newTab(std::shared_ptr<Buffer> buf = {});
  TabBuffer *numTab(int n) const;
  void drawTabs();
  void nextTab();
  void prevTab();
  void tabRight();
  void tabLeft();
  int calcTabPos();
  TabBuffer *deleteTab(TabBuffer *tab);
  void moveTab(TabBuffer *t, TabBuffer *t2, int right);
  void pushBuffer(const std::shared_ptr<Buffer> &buf, std::string_view target);

  struct GeneralList *message_list = NULL;
  char *delayed_msg = NULL;
  void message(const char *s, int return_x, int return_y);

  void record_err_message(const char *s);
  std::shared_ptr<Buffer> message_list_panel();
  void disp_err_message(const char *s);
  void disp_message_nsec(const char *s, int sec, int purge);
  void disp_message(const char *s);
  void set_delayed_message(const char *s);

  void refresh_message();

  void showProgress(long long *linelen, long long *trbyte,
                    long long current_content_length);
};
#define CurrentTab App::instance().CurrentTab()

char *convert_size(long long size, int usefloat);
char *convert_size2(long long size1, long long size2, int usefloat);
