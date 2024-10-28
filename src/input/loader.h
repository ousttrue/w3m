#pragma once
#include "istream.h"

extern const char *DefaultType;
extern bool UseExternalDirBuffer;
extern bool label_topline;
extern bool retryAsHttp;
extern bool AutoUncompress;
extern int FollowRedirection;
extern const char *DirBufferCommand;

struct Url;
struct FormList;
struct Buffer;

/*
 * loadGeneralFile: load file to buffer
 */
struct Buffer *loadGeneralFile(int cols, const char *path, struct Url *current,
                               const char *referer, enum RG_FLAGS flag,
                               struct FormList *request);
