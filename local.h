/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_LOCAL_H
#define W3M_LOCAL_H

#include <sys/stat.h>
#include <dirent.h>
#include "Str.h"

#define NOT_REGULAR(m)  (((m) & S_IFMT) != S_IFREG)
#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

Str loadLocalDir(const char *dirname);

#endif
