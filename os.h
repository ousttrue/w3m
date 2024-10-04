#pragma once
#include "url_stream.h"

struct Buffer;
struct URLFile;

void myExec(char *command);
Str myExtCommand(const char *cmd, const char *arg, int redirect);
Str myEditor(const char *cmd, const char *file, int line);
void mySystem(char *command, int background);
int _doFileCopy(const char *tmpf, const char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
int doFileSave(struct URLFile uf, const char *defstr);
