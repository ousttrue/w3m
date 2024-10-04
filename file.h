#pragma once
#include "Str.h"

const char *file_quote(const char *str);
const char *file_unquote(const char *str);

int _MoveFile(char *path1, char *path2);
int getMetaRefreshParam(const char *q, Str *refresh_uri);
struct Buffer *getshell(const char *cmd);
int checkOverWrite(const char *path);

const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
char *guess_filename(const char *file);

bool dir_exist(const char *path);
const char *currentdir();
