#include "core.h"
#include "alloc.h"
#include "file/file.h"
#include "file/tmpfile.h"
#include <stdlib.h>

#ifdef _WIN32
#include <process.h>
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

const char *CurrentDir = nullptr;
const char *getCurrentDir() { return CurrentDir; }

uint32_t CurrentPid = 0;
uint32_t getCurrentPid() { return CurrentPid; }

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

void initialize() {
  CurrentDir = currentdir();
#ifdef _WIN32
  CurrentPid = GetCurrentProcessId();
#else
  CurrentPid = (int)getpid();
#endif
  tmpfile_init();
}
