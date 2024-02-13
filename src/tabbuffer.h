#pragma once
#include "display_flag.h"
#include "url.h"
#include <optional>
#include <gc_cpp.h>
#include <memory>

struct Buffer;
struct FormList;
struct FormItemList;
struct Anchor;
struct TabBuffer : public gc_cleanup {
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

  static void _newT();
  static int calcTabPos(DisplayFlag mode);
};

extern TabBuffer *CurrentTab;
struct LineLayout;
void SAVE_BUFPOSITION(LineLayout *sbufp);
void RESTORE_BUFPOSITION(const LineLayout &sbufp);

extern TabBuffer *FirstTab;
extern TabBuffer *LastTab;
extern bool open_tab_blank;
extern bool open_tab_dl_list;
extern bool close_tab_back;
extern int nTab;
extern int TabCols;
extern bool check_target;

TabBuffer *deleteTab(TabBuffer *tab);
TabBuffer *numTab(int n);
void moveTab(TabBuffer *t, TabBuffer *t2, int right);
void tabURL0(TabBuffer *tab, const char *prompt, int relative);
void followTab(TabBuffer *tab);
struct Str;
void gotoLabel(const char *label);
void goURL0(const char *prompt, int relative);
