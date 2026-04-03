#pragma once
#include "Str.h"
#include <stdio.h>
#include <sys/types.h>

#define DEV_NULL_PATH "/dev/null"

pid_t open_pipe_rw(FILE** fr, FILE** fw);
FILE* openSecretFile(const char* fname);
char* lastFileName(const char* path);

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define TMPF_HIST 5
#define MAX_TMPF_TYPE 6

Str tmpfname(int type, const char* ext);
int next_status(char c, int* status);
char* mydirname(const char* s);
char* mybasename(const char* s);
Str myEditor(const char* cmd, const char* file, int line);
Str myExtCommand(const char* cmd, const char* arg, int redirect);
void setup_child(int child, int i, int f);
void myExec(const char* command);
Str base64_encode(const char* src, size_t len);
int is_localhost(const char* host);
time_t mymktime(const char* timestr);
