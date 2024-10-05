#pragma once
#include "url_stream.h"

extern const char *DefaultType;
extern bool use_proxy;
extern const char *FTP_proxy;
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
struct Buffer *loadGeneralFile(const char *path, struct Url *current,
                               const char *referer, enum RG_FLAGS flag,
                               struct FormList *request);

typedef struct Buffer *(*LoadProc)(struct URLFile *, const char *,
                                   struct Buffer *);
struct Buffer *loadcmdout(const char *cmd, LoadProc loadproc,
                          struct Buffer *defaultbuf);
