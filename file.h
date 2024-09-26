#pragma once
#include "fm.h"

Buffer *loadcmdout(char *cmd, Buffer *(*loadproc)(URLFile *, Buffer *),
                   Buffer *defaultbuf);

void uncompress_stream(URLFile *uf, char **src);
