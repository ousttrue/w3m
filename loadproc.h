#pragma once
struct Buffer;
struct URLFile;
typedef struct Buffer *(*LoadProc)(struct URLFile *, const char *,
                                   struct Buffer *);
