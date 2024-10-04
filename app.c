#include "app.h"
#include "tmpfile.h"
#include "file.h"
#include "alloc.h"
#include <stdlib.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

const char *CurrentDir = nullptr;
const char *app_currentdir() { return CurrentDir; }

int CurrentPid = -1;

struct Event *CurrentEvent = NULL;
static struct Event *LastEvent = NULL;

void pushEvent(int cmd, void *data) {
  struct Event *event;

  event = New(struct Event);
  event->cmd = cmd;
  event->data = data;
  event->next = NULL;
  if (CurrentEvent)
    LastEvent->next = event;
  else
    CurrentEvent = event;
  LastEvent = event;
}

void app_init() {

  CurrentDir = currentdir();
  CurrentPid = (int)getpid();

  tmpfile_init();
}
