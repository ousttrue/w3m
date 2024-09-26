#pragma once
#include "fm.h"

struct Buffer *
loadcmdout(char *cmd, struct Buffer *(*loadproc)(URLFile *, struct Buffer *),
           struct Buffer *defaultbuf);

void uncompress_stream(URLFile *uf, char **src);
