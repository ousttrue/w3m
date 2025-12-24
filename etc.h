#pragma once
#include "Str.h"
#include <sys/types.h>

Str base64_encode(const char* src, size_t len);
char* lastFileName(const char* path);
char* mydirname(const char* s);
char* mybasename(const char* s);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
