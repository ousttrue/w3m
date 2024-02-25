#pragma once
#include "url.h"
#include <optional>
#include <gc_cpp.h>
#include <memory>

extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int TabCols;

struct Buffer;
struct FormList;
struct FormItemList;
struct Anchor;
struct HttpOption;

struct TabBuffer {
  // friend class App;
  std::shared_ptr<TabBuffer> nextTab;
  std::shared_ptr<TabBuffer> prevTab;

  short x1 = 0;
  short x2 = 0;
  short y = 0;
  std::shared_ptr<Buffer> _currentBuffer;

private:
  TabBuffer();

public:
  std::shared_ptr<Buffer> firstBuffer;

  ~TabBuffer();
  TabBuffer(const TabBuffer &) = delete;
  TabBuffer &operator=(const TabBuffer &) = delete;
  static std::shared_ptr<TabBuffer> create() {
    return std::shared_ptr<TabBuffer>(new TabBuffer);
  }

  std::shared_ptr<Buffer> currentBuffer() { return _currentBuffer; }
  void currentBuffer(const std::shared_ptr<Buffer> &newbuf,
                     bool first = false) {
    _currentBuffer = newbuf;
    if (first) {
      firstBuffer = newbuf;
    }
  }
  std::shared_ptr<Buffer> forwardBuffer(const std::shared_ptr<Buffer> &buf);

  void deleteBuffer(const std::shared_ptr<Buffer> &delbuf);
  const std::shared_ptr<Buffer> &namedBuffer(const char *name);
  void repBuffer(const std::shared_ptr<Buffer> &oldbuf,
                 const std::shared_ptr<Buffer> &buf);
  void pushBuffer(const std::shared_ptr<Buffer> &buf);
  // bool select(char cmd, const std::shared_ptr<Buffer> &buf);

  std::shared_ptr<Buffer> replaceBuffer(const std::shared_ptr<Buffer> &delbuf,
                                        const std::shared_ptr<Buffer> &newbuf);

  int draw(class Screen *screen, TabBuffer *current);
};

// void followTab();
