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
struct TabBuffer : public gc_cleanup {
  friend class App;
  TabBuffer *nextTab = nullptr;
  TabBuffer *prevTab = nullptr;

private:
  std::shared_ptr<Buffer> _currentBuffer;
  short x1 = 0;
  short x2 = 0;
  short y = 0;

public:
  std::shared_ptr<Buffer> firstBuffer;

  TabBuffer();
  ~TabBuffer();
  TabBuffer(const TabBuffer &) = delete;
  TabBuffer &operator=(const TabBuffer &) = delete;

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
  bool select(char cmd, const std::shared_ptr<Buffer> &buf);

  void cmd_loadURL(const char *url, std::optional<Url> current,
                   const HttpOption &option, FormList *request);

  std::shared_ptr<Buffer> replaceBuffer(const std::shared_ptr<Buffer> &delbuf,
                                        const std::shared_ptr<Buffer> &newbuf);

  void goURL0(const char *prompt, bool relative);

  int draw(class Screen *screen, TabBuffer *current);
};

// void followTab();
