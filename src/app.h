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
#include <ftxui/component/event.hpp>
#include <sstream>

extern bool displayLink;
extern bool displayLineInfo;
extern int prev_key;
extern std::string CurrentKeyData;
extern std::string CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;
extern std::string keymap_file;

using Dispatcher = std::function<bool(const char *buf, size_t len)>;

class Content;
struct TabBuffer;
struct Buffer;
class App {
  RowCol _size = {};

  std::stringstream _lastKeyCmd;
  std::list<ftxui::Element> _event_status;
  std::string _message;

  bool _fmInitialized = false;

  int _currentPid = -1;

  std::unordered_map<std::string, Func> _w3mFuncList;
  char _currentKey = -1;
  char _prev_key = -1;

  std::list<std::shared_ptr<TabBuffer>> _tabs;
  int _currentTab = 0;

  std::stack<Dispatcher> _dispatcher;

  std::array<std::string, 128> _GlobalKeymap;
  std::unordered_map<std::string, std::string> _keyData;
  std::string _last;
  std::shared_ptr<Buffer> _save_current_buf;

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
  bool initialize();
  void beginRawMode();
  void endRawMode();
  bool isRawMode() const { return _fmInitialized; }
  int mainLoop();
  bool onEvent(const ftxui::Event &event);
  void exit(int rval = 0);

  int pid() const { return _currentPid; }

  std::string searchKeyData();
  int searchKeyNum();

  mutable std::string _peekUrl;
  mutable int _peekUrlOffset = 0;
  void peekURL();

  void doCmd();
  void doCmd(const std::string &cmd, const std::string_view data);
  void dispatchPtyIn(const char *buf, size_t len);
  void dispatch(const char *buf, size_t len) {
    if (!_dispatcher.top()(buf, len)) {
      _dispatcher.pop();
    }
  }

  void initKeymap(bool force);
  void setKeymap(const std::string &p, int lineno, int verbose);
  std::string getKeyData(char key) {
    auto func = _GlobalKeymap[key];
    auto found = _keyData.find(func);
    if (found == _keyData.end()) {
      return {};
    }
    return found->second;
  }

  FuncContext context() {
    return {
        // .curretTab = currentTab(),
    };
  }

  void pushDispatcher(const Dispatcher &dispatcher) {
    _dispatcher.push(dispatcher);
  }
  ftxui::Element dom();
  ftxui::Element tabs();
  void task(int sec, const std::string &cmd, std::string_view data = {},
            bool releat = false);

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
  void nextTab(int n);
  void prevTab(int n);
  void tabRight(int n);
  void tabLeft(int n);
  int calcTabPos();
  void deleteTab(const std::shared_ptr<TabBuffer> &tab);
  void pushBuffer(const std::shared_ptr<Buffer> &buf, std::string_view target);

  std::list<std::string> message_list;
  std::string delayed_msg;
  void message(const std::string &s);
  void record_err_message(const std::string &s);
  std::string message_list_panel();
  void disp_err_message(const std::string &s);
  void disp_message_nsec(const std::string &s, int sec, int purge);
  void disp_message(const std::string &s);
  void set_delayed_message(std::string_view s);

  void refresh_message();

  std::string make_lastline_link(const std::shared_ptr<Buffer> &buf,
                                 const char *title, const char *url);
};
#define CurrentTab App::instance().currentTab()
