#pragma once
#include "fm.h"

extern int64_t current_content_length;

struct Buffer *loadcmdout(char *cmd,
                          struct Buffer *(*loadproc)(struct URLFile *,
                                                     struct Buffer *),
                          struct Buffer *defaultbuf);

void uncompress_stream(struct URLFile *uf, char **src);

int _MoveFile(char *path1, char *path2);
