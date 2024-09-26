#include "localcgi.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void set_environ(const char *var, const char *value) {
  SetEnvironmentVariable(var, value);
}
