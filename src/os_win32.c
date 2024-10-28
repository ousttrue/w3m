#include "os.h"
#include <stdlib.h>
#include <synchapi.h>

void mySystem(char *command, int background) {}

int _doFileCopy(const char *tmpf, const char *defstr, int download) {
  abort();
  return -1;
}

int doFileSave(union input_stream *stream, const char *defstr) {
  abort();
  return -1;
}

pid_t open_pipe_rw(FILE **fr, FILE **fw) { return -1; }

const char *expandName(const char *name) { return name; }

void sleepSeconds(uint32_t seconds) { Sleep(seconds * 1000); }
