#pragma once
#include "text/Str.h"

int _MoveFile(char *path1, char *path2);
int getMetaRefreshParam(const char *q, Str *refresh_uri);
int checkOverWrite(const char *path);

const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
char *guess_filename(const char *file);

bool dir_exist(const char *path);
const char *currentdir();
