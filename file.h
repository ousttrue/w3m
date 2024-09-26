#pragma once
#include "fm.h"

struct Buffer *
loadcmdout(char *cmd, struct Buffer *(*loadproc)(struct URLFile *, struct Buffer *),
           struct Buffer *defaultbuf);

void uncompress_stream(struct URLFile *uf, char **src);
