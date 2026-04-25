#pragma once
#include "Str.h"

#define HAVE_LSTAT 1
#define HAVE_READLINK 1

#include <sys/types.h>
#define HAVE_DIRENT_H 1
#ifdef HAVE_DIRENT_H
#include <dirent.h>
typedef struct dirent Directory;
#else /* not HAVE_DIRENT_H */
#include <sys/dir.h>
typedef struct direct Directory;
#endif /* not HAVE_DIRENT_H */
#include <sys/stat.h>

#ifndef S_IFMT
#define S_IFMT 0170000
#endif /* not S_IFMT */
#ifndef S_IFREG
#define S_IFREG 0100000
#endif /* not S_IFREG */


#ifndef S_ISDIR
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif /* not S_IFDIR */
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif /* not S_ISDIR */

#ifdef HAVE_READLINK
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif /* not S_IFLNK */
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif /* not S_ISLNK */
#endif /* not HAVE_READLINK */

Str localCookie(void);
Str loadLocalDir(const char* dirname);
struct Form;
FILE* localcgi_post(const char*, const char*, struct Form*, const char*);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
