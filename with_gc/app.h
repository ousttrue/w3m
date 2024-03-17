#pragma once
#include "rowcol.h"
#include "func.h"
#include <string>
#include <list>
#include <memory>
#include <functional>
#include <stack>
#include <array>
#include <unordered_map>
#include <ftxui/screen/terminal.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <sstream>

extern bool displayLink;
extern bool displayLineInfo;
extern int prev_key;
extern const char *CurrentKeyData;
extern const char *CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;
extern std::string keymap_file;

enum TmpfType {
  TMPF_DFL = 0,
  TMPF_SRC = 1,
  TMPF_FRAME = 2,
  TMPF_CACHE = 3,
  TMPF_COOKIE = 4,
  TMPF_HIST = 5,
  MAX_TMPF_TYPE = 6,
};

using Dispatcher = std::function<bool(const char *buf, size_t len)>;

class Content;
struct TabBuffer;
struct Buffer;
class App {
  RowCol _size = {};

  std::stringstream _lastKeyCmd;
  std::string _status;
  std::string _message;

  bool _fmInitialized = false;

  std::list<std::string> _fileToDelete;

  int _currentPid = -1;

  std::unordered_map<std::string, Func> w3mFuncList;
  char _currentKey = -1;
  char prev_key = -1;

  std::list<std::shared_ptr<TabBuffer>> _tabs;
  int _currentTab = 0;

  std::stack<Dispatcher> _dispatcher;

  std::array<std::string, 128> GlobalKeymap;
  std::unordered_map<std::string, std::string> keyData;
  std::string _last;
  std::shared_ptr<Buffer> save_current_buf;

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

  bool initialize();
  void beginRawMode();
  void endRawMode();
  bool isRawMode() const { return _fmInitialized; }
  int mainLoop();
  void exit(int rval = 0);

  int pid() const { return _currentPid; }

  const char *searchKeyData();
  int searchKeyNum();

  mutable std::string _peekUrl;
  mutable int _peekUrlOffset = 0;
  void peekURL();

  mutable std::string _currentUrl;
  mutable int _currentUrlOffset = 0;
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

  FuncContext context() {
    return {
        .curretTab = currentTab(),
    };
  }

  void pushDispatcher(const Dispatcher &dispatcher) {
    _dispatcher.push(dispatcher);
  }
  void onFrame();
  void cursor(const RowCol &pos);
  void display();
  ftxui::Element dom();
  ftxui::Element tabs();
  void task(int sec, const std::string &cmd, const char *data = nullptr,
            bool releat = false);
  std::string tmpfname(TmpfType type, const std::string &ext);

  // tabs
  std::shared_ptr<TabBuffer> FirstTab() const { return _tabs.front(); }
  std::shared_ptr<TabBuffer> LastTab() const { return _tabs.back(); }
  int nTab() const { return _tabs.size(); }
  std::shared_ptr<TabBuffer> currentTab() const {
    auto it = _tabs.begin();
    for (int i = 0; i < _currentTab && it != _tabs.end(); ++i, ++it) {
    }
    return *it ? *it : _tabs.back();
  }
  std::shared_ptr<TabBuffer> newTab(std::shared_ptr<Buffer> buf = {});
  std::shared_ptr<TabBuffer> numTab(int n) const;
  // void drawTabs();
  void nextTab(int n);
  void prevTab(int n);
  void tabRight(int n);
  void tabLeft(int n);
  int calcTabPos();
  void deleteTab(const std::shared_ptr<TabBuffer> &tab);
  void pushBuffer(const std::shared_ptr<Buffer> &buf, std::string_view target);

  std::list<std::string> message_list;
  char *delayed_msg = NULL;
  void message(const char *s);
  void record_err_message(const char *s);
  std::shared_ptr<Buffer> message_list_panel(int width);
  void disp_err_message(const char *s);
  void disp_message_nsec(const char *s, int sec, int purge);
  void disp_message(const char *s);
  void set_delayed_message(const char *s);

  void refresh_message();

  void showProgress(long long *linelen, long long *trbyte,
                    long long current_content_length);
  std::string make_lastline_link(const std::shared_ptr<Buffer> &buf,
                                 const char *title, const char *url);
  std::string make_lastline_message(const std::shared_ptr<Buffer> &buf);
};
#define CurrentTab App::instance().currentTab()

char *convert_size(long long size, int usefloat);
char *convert_size2(long long size1, long long size2, int usefloat);
