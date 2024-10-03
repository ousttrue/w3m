#pragma once
#include "url_stream.h"
#include "loadproc.h"

struct Url;
struct FormList;
struct Buffer;

/*
 * loadGeneralFile: load file to buffer
 */
struct Buffer *loadGeneralFile(const char *path, struct Url *current,
                               const char *referer, enum RG_FLAGS flag,
                               struct FormList *request);

struct Buffer *loadcmdout(const char *cmd, LoadProc loadproc,
                          struct Buffer *defaultbuf);
