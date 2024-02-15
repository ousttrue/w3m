#pragma once
#include <stdio.h>

#include <sys/types.h>

extern char *cgi_bin;

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

void set_environ(const char *var, const char *value);
struct Str;
Str *localCookie(void);
struct FormList;

struct HttpOption;
FILE *localcgi_post(const char *uri, const char *qstr, FormList *request,
                    const HttpOption &option);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
