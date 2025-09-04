#pragma once
#include <Str.h>
#include <sys/types.h>

struct form_list;

// #define DEV_NULL_PATH "nul"
#define DEV_NULL_PATH "/dev/null"

#ifdef _WIN32
#else /* not HAVE_DIRENT_H */
#include <dirent.h>
typedef struct dirent Directory;
#endif /* not HAVE_DIRENT_H */

#include <sys/stat.h>

#ifndef S_IFMT
#define S_IFMT 0170000
#endif /* not S_IFMT */
#ifndef S_IFREG
#define S_IFREG 0100000
#endif /* not S_IFREG */

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

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

extern char* HostName;
extern int multicolList;
extern char* cgi_bin;
extern char* personal_document_root;

Str localCookie(void);
Str loadLocalDir(char* dirname);
void set_environ(const char* var, const char* value);

FILE* localcgi_post(char*, char*, struct form_list*, const char*);

static inline FILE* localcgi_get(char* u, char* q, const char* r)
{
    return localcgi_post((u), (q), NULL, (r));
}
