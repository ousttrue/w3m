#pragma once
#include "url_stream.h"

struct Buffer;
struct URLFile;

void mySystem(char *command, int background);
int _doFileCopy(char *tmpf, char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, false);
struct Buffer *doExternal(struct URLFile uf, char *type,
                          struct Buffer *defaultbuf);
int doFileSave(struct URLFile uf, char *defstr);
