#include "filepath.h"
#include "alloc.h"
#include <string.h>

#define DEF_SAVE_FILE "index.html"

const char* fpath_basename(const char* s)
{
    const char* p = s;
    while (*p)
        p++;
    while (s <= p && *p != '/')
        p--;
    if (*p == '/')
        p++;
    else
        p = s;
    return p;
}

char* alloc_guess_filename(const char* file)
{
    char* p = NULL;
    if (file != NULL)
        p = allocStr(fpath_basename(file), -1);
    if (p == NULL || *p == '\0')
        return DEF_SAVE_FILE;

    char* s = p;
    if (*p == '#')
        p++;
    while (*p != '\0') {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
            // terminate '?'
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}
