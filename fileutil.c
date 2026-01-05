#include "fileutil.h"
#include <sys/stat.h>

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

bool dir_exist(const char* path)
{
    if (!path || *path == '\0')
        return false;
    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return false;

    return IS_DIRECTORY(stbuf.st_mode);
}
