#pragma once
#include "url_stream.h"
#include "loadproc.h"

struct Url;
struct FormList;
struct Buffer;

/*
 * loadGeneralFile: load file to buffer
 */
struct Buffer *loadGeneralFile(char *path, struct Url *current, char *referer,
                               enum RG_FLAGS flag, struct FormList *request);

struct Buffer *loadcmdout(char *cmd, LoadProc loadproc,
                          struct Buffer *defaultbuf);
