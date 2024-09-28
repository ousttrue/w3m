#pragma once

struct Url;
struct URLFile;
struct FormList;
struct Buffer;
/*
 * loadGeneralFile: load file to buffer
 */
struct Buffer *loadGeneralFile(char *path, struct Url *current, char *referer,
                               int flag, struct FormList *request);

struct Buffer *loadcmdout(char *cmd,
                          struct Buffer *(*loadproc)(struct URLFile *,
                                                     struct Buffer *),
                          struct Buffer *defaultbuf);
