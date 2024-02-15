#pragma once
#include "url.h"
#include <optional>
#include <gc_cpp.h>
#include <memory>

extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int TabCols;
extern bool check_target;

struct Buffer;
struct FormList;
struct FormItemList;
struct Anchor;
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

  void deleteBuffer(const std::shared_ptr<Buffer> &delbuf);
  const std::shared_ptr<Buffer> &namedBuffer(const char *name);
  void repBuffer(const std::shared_ptr<Buffer> &oldbuf,
                 const std::shared_ptr<Buffer> &buf);
  void pushBuffer(const std::shared_ptr<Buffer> &buf);
  bool select(char cmd, const std::shared_ptr<Buffer> &buf);

  void cmd_loadURL(const char *url, std::optional<Url> current,
                   const char *referer, FormList *request);

  std::shared_ptr<Buffer> replaceBuffer(const std::shared_ptr<Buffer> &delbuf,
                                        const std::shared_ptr<Buffer> &newbuf);

  std::shared_ptr<Buffer> loadLink(const char *url, const char *target,
                                   const char *referer, FormList *request);
  void do_submit(FormItemList *fi, Anchor *a);
  void _followForm(int submit);
  int draw();
};

struct LineLayout;
void SAVE_BUFPOSITION(LineLayout *sbufp);
void RESTORE_BUFPOSITION(const LineLayout &sbufp);

void tabURL0(const char *prompt, int relative);
void followTab();
struct Str;
void gotoLabel(const char *label);
void goURL0(const char *prompt, int relative);
