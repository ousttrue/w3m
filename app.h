#pragma once
#include "Str.h"

extern const char *CurrentDir;
extern int CurrentPid;
struct TextList;
extern struct TextList *fileToDelete;
extern const char *w3m_reqlog;

struct Event {
  int cmd;
  void *data;
  struct Event *next;
};
extern struct Event *CurrentEvent;

enum TMPF_TYPE {
  TMPF_DFL = 0,
  TMPF_SRC = 1,
  TMPF_FRAME = 2,
  TMPF_CACHE = 3,
  TMPF_COOKIE = 4,
  MAX_TMPF_TYPE = 5,
};

void app_init();
void app_no_rcdir(const char *rcdir);
void app_set_tmpdir(const char *dir);
const char *app_get_tmpdir();
Str tmpfname(enum TMPF_TYPE type, const char *ext);
const char *app_currentdir();

void pushEvent(int cmd, void *data);
