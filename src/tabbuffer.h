#pragma once
#include "url.h"
#include <optional>
#include <gc_cpp.h>
#include <memory>

struct Buffer;
struct FormList;

struct TabBuffer : public gc_cleanup {
  TabBuffer *nextTab = nullptr;
  TabBuffer *prevTab = nullptr;

private:
  std::shared_ptr<Buffer> _currentBuffer;

public:
  std::shared_ptr<Buffer> firstBuffer;
  short x1 = 0;
  short x2 = 0;
  short y = 0;

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

  static void init(const std::shared_ptr<Buffer> &newbuf);
  static void _newT();
};
#define NO_TABBUFFER ((TabBuffer *)1)

extern TabBuffer *CurrentTab;
#define Currentbuf (CurrentTab->currentBuffer())
#define Firstbuf (CurrentTab->firstBuffer)
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

void calcTabPos();
TabBuffer *deleteTab(TabBuffer *tab);
TabBuffer *numTab(int n);
void moveTab(TabBuffer *t, TabBuffer *t2, int right);
void tabURL0(TabBuffer *tab, const char *prompt, int relative);
void saveBufferInfo();
void followTab(TabBuffer *tab);
struct Str;
Str *currentURL(void);
void gotoLabel(const char *label);
int handleMailto(const char *url);
void goURL0(const char *prompt, int relative);
