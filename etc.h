#pragma once
#include "Str.h"
#include <sys/types.h>

Str base64_encode(const char* src, size_t len);
char* lastFileName(const char* path);
char* mydirname(const char* s);
char* mybasename(const char* s);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
time_t mymktime(const char* timestr);

enum TmpFileTypes {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_FRAME = 2,
    TMPF_CACHE = 3,
    TMPF_COOKIE = 4,
    TMPF_HIST = 5,
    MAX_TMPF_TYPE = 6,
};

Str tmpfname(enum TmpFileTypes type, const char* ext);
char* file_to_url(const char* file);
void setup_child(int child, int i, int f);
int gethtmlcmd(const char** s);
