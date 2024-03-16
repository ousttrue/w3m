#pragma once
#include <stdio.h>
#include <string>
#include <sys/types.h>

extern std::string document_root;
extern std::string cgi_bin;

// #ifndef S_IFMT
// #define S_IFMT 0170000
// #endif /* not S_IFMT */
// #ifndef S_IFREG
// #define S_IFREG 0100000
// #endif /* not S_IFREG */

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

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
