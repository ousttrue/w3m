#pragma once
#include "fm.h"

int _MoveFile(char *path1, char *path2);
int getMetaRefreshParam(const char *q, Str *refresh_uri);
struct Buffer *getshell(const char *cmd);
int checkOverWrite(const char *path);
