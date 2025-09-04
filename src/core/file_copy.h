#pragma once
#include <stdbool.h>
#include <time.h>

extern bool PermitSaveToPipe;

int setModtime(const char* path, time_t modtime);
bool notExistsOrOverWrite(const char* path);
int _doFileCopy(char* tmpf, char* defstr, int download);
inline static int doFileCopy(char* tmpf, char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);
