#pragma once
#include <Str.h>

enum TmpFileType {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_CACHE = 2,
    TMPF_COOKIE = 3,
    TMPF_HIST = 4,
    MAX_TMPF_TYPE = 5,
};

void initDeleteFile();
void deinitDeleteFile();
void pushDeleteFile(const char* path);
Str tmpfname(enum TmpFileType type, const char* ext);
