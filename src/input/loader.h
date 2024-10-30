#pragma once

extern bool UseExternalDirBuffer;
extern bool label_topline;
extern bool AutoUncompress;
extern const char *DirBufferCommand;

struct Url;
struct FormList;
struct Buffer;
struct Buffer *loadGeneralFile(int cols, const char *path, struct Url *current,
                               const char *referer, bool no_cache,
                               struct FormList *form);
