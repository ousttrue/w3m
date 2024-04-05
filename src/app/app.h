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
#include <iomanip>
#include <plog/Log.h> // Step1: include the header.
#include <plog/Formatters/FuncMessageFormatter.h>
#include <ftxui/dom/table.hpp>

extern bool displayLink;
extern bool displayLineInfo;
extern int prev_key;
extern std::string CurrentKeyData;
extern std::string CurrentCmdData;
extern bool on_target;
extern std::string keymap_file;

using Dispatcher = std::function<bool(const char *buf, size_t len)>;

class Content;
struct TabBuffer;
struct Buffer;

namespace plog {

inline ftxui::Color::Palette256 serverityColor(Severity s) {
  switch (s) {
  case none:
    return ftxui::Color::Palette256::NavajoWhite1;
  case fatal:
    return ftxui::Color::Palette256::Magenta1;
  case error:
    return ftxui::Color::Palette256::Red1;
  case warning:
    return ftxui::Color::Palette256::Orange1;
  case info:
    return ftxui::Color::Palette256::NavajoWhite1;
  case debug:
    return ftxui::Color::Palette256::DarkSlateGray1;
  case verbose:
    return ftxui::Color::Palette256::DarkSlateGray2;
  }

  return ftxui::Color::Palette256::Purple;
}
// color;
// enum Severity
// {
//     none = 0,
// };

template <class Formatter> // Typically a formatter is passed as a template
                           // parameter.
class MyAppender
    : public IAppender // All appenders MUST inherit IAppender interface.
{
  ftxui::Table _table;
  ftxui::Element _dom;

  // std::list<util::nstring> m_messageList;
  std::vector<std::vector<std::string>> _data = {{
      "servirty",
      // "time",
      "func",
      "message",
  }};
  std::vector<Severity> _records;

public:
  MyAppender() {}

  int _height = 0;

  virtual void write(const Record &record)
      PLOG_OVERRIDE // This is a method from IAppender that MUST be implemented.
  {
    _records.push_back(record.getSeverity());

    // util::nstring str = Formatter::format(
    //     record); // Use the formatter to get a string from a record.
    //
    // m_messageList.push_back(str); // Store a log message in a list.

    tm t;
    util::localtime_s(&t, &record.getTime().time);

    // auto t= record.getTime();
    std::stringstream ss;
    // ss //<< t.tm_year + 1900 << "-" << std::setfill(PLOG_NSTR('0'))
    // << std::setw(2) << t.tm_mon + 1 << PLOG_NSTR("-")
    //    << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_mday
    //    << PLOG_NSTR(" ");
    ss << std::setfill(PLOG_NSTR('0')) << std::setw(2) << t.tm_hour
       << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0')) << std::setw(2)
       << t.tm_min << PLOG_NSTR(":") << std::setfill(PLOG_NSTR('0'))
       << std::setw(2) << t.tm_sec
        // << PLOG_NSTR(".")
        // << std::setfill(PLOG_NSTR('0')) << std::setw(3)
        //    << static_cast<int>(record.getTime().millitm) << PLOG_NSTR(" ");
        // ss << std::setfill(PLOG_NSTR(' ')) << std::setw(5) << std::left
        //    << severityToString(record.getSeverity()) << PLOG_NSTR(" ");
        ;

    std::vector<std::string> row = {
        severityToString(record.getSeverity()),
        // ss.str(),
        std::string(record.getFunc()),
        std::string(record.getMessage()),
    };
    _data.push_back(row);
    if (_height) {
      auto d = (int)_data.size() - _height;
      if (d > 0) {
        _data.erase(_data.begin() + 1, _data.begin() + 1 + d);
        _records.erase(_records.begin(), _records.begin() + d);
      }
    }

    _table = ftxui::Table(_data);

    _table.SelectRow(0).Decorate(ftxui::bold);
    _table.SelectRow(0).SeparatorVertical(ftxui::LIGHT);
    // _table.SelectRow(0).Border(ftxui::DOUBLE);

    auto content = _table.SelectRows(1, -1);
    // content.DecorateCellsAlternateRow(color(ftxui::Color::Blue), 2, 0);
    // content.DecorateCellsAlternateRow(color(ftxui::Color::Cyan), 2, 1);

    for (int i = 0; i < _records.size(); ++i) {
      _table.SelectRow(i + 1).Decorate(color(serverityColor(_records[i])));
    }

    _dom = _table.Render();
  }

  ftxui::Element dom() { return _dom; }
};
} // namespace plog

class App {
  int _splitSize = 20;
  plog::MyAppender<plog::FuncMessageFormatter>
      _appender; // Create our custom appender.

  RowCol _size = {};

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
  bool isRawMode() const { return _fmInitialized; }
  int mainLoop();
  bool onEvent(ftxui::ScreenInteractive *screen, const ftxui::Event &event);

  int pid() const { return _currentPid; }

  std::string searchKeyData();

  mutable std::string _peekUrl;
  mutable int _peekUrlOffset = 0;
  void peekURL();

  void doCmd();
  void doCmd(const std::string &cmd, const std::string_view data);
  bool dispatchPtyIn(const char *buf, size_t len);
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
