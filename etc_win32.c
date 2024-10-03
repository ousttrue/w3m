#include "etc.h";
#include <synchapi.h>

pid_t open_pipe_rw(FILE **fr, FILE **fw) { return -1; }

const char *expandName(const char *name) { return name; }

void sleepSeconds(uint32_t seconds) { Sleep(seconds * 1000); }
