#pragma once
#include "fm.h"


struct Buffer *loadcmdout(char *cmd,
                          struct Buffer *(*loadproc)(struct URLFile *,
                                                     struct Buffer *),
                          struct Buffer *defaultbuf);

int _MoveFile(char *path1, char *path2);

