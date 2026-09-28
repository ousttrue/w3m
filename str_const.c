#include "str_const.h"
#include <string.h>

const char* mybasename(const char* path)
{
    const char* p = strrchr(path, '/');
    return p ? p + 1 : path;
}
