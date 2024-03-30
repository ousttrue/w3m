#pragma once
#include <time.h>

extern bool PermitSaveToPipe;
extern bool PreserveTimestamp;

int _MoveFile(const char *path1, const char *path2);
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);


bool couldWrite(const char *path);

int setModtime(const char *path, time_t modtime);
