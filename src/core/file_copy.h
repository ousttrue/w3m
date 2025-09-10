#pragma once
#include <stdbool.h>
#include <time.h>

extern bool PermitSaveToPipe;

bool canCopyFile(const char* path1, const char* path2);
int setModtime(const char* path, time_t modtime);
int _doFileCopy(const char* tmpf, const char* defstr, int download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);
