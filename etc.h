#pragma once
#include "Str.h"
#include <stdio.h>
#include <sys/types.h>

#define DEV_NULL_PATH "/dev/null"

pid_t open_pipe_rw(FILE** fr, FILE** fw);
char* lastFileName(const char* path);

enum TmpFileType {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_FRAME = 2,
    TMPF_CACHE = 3,
    TMPF_COOKIE = 4,
    TMPF_HIST = 5,
    MAX_TMPF_TYPE = 6,
};
const char* tmpfname(enum TmpFileType type, const char* ext);

char* mydirname(const char* s);
char* mybasename(const char* s);
Str myEditor(const char* cmd, const char* file, int line);
Str myExtCommand(const char* cmd, const char* arg, int redirect);
void setup_child(int child, int i, int f);
void myExec(const char* command);
Str base64_encode(const char* src, size_t len);
int is_localhost(const char* host);
time_t mymktime(const char* timestr);
char* file_to_url(const char* file);
