#pragma once
#include "input/istream.h"
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <process.h>
typedef long long pid_t;
#else
// pid_t
#include <unistd.h>
#endif

struct Buffer;
struct URLFile;

void myExec(char *command);
Str myExtCommand(const char *cmd, const char *arg, int redirect);
Str myEditor(const char *cmd, const char *file, int line);
void mySystem(char *command, int background);
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
int doFileSave(struct URLFile uf, const char *defstr);
pid_t open_pipe_rw(FILE **fr, FILE **fw);
const char *expandName(const char *name);
void sleepSeconds(uint32_t seconds);
