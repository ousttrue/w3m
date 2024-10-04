#pragma once
#include "Str.h"

const char *w3m_auxbin_dir();
const char *w3m_lib_dir();
const char *w3m_etc_dir();
const char *w3m_conf_dir();
const char *w3m_help_dir();
const char *etcFile(const char *base);
const char *confFile(const char *base);
const char *expandPath(const char *name);

int _MoveFile(char *path1, char *path2);
int getMetaRefreshParam(const char *q, Str *refresh_uri);
struct Buffer *getshell(const char *cmd);
int checkOverWrite(const char *path);

const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
char *guess_filename(const char *file);

bool dir_exist(const char *path);
