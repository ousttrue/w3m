#pragma once
#include "Str.h"
#include <stdio.h>
#include <sys/types.h>

#define DEV_NULL_PATH "/dev/null"

pid_t open_pipe_rw(FILE** fr, FILE** fw);
char* lastFileName(const char* path);

char* mydirname(const char* s);
Str myEditor(const char* cmd, const char* file, int line);
Str myExtCommand(const char* cmd, const char* arg, int redirect);
void setup_child(int child, int i, int f);
void myExec(const char* command);
Str base64_encode(const char* src, size_t len);
time_t mymktime(const char* timestr);
Str unescape_spaces(Str s);
