#pragma once
#include <gcstr/Str.h>
#include "config.h"
#include <stdlib.h>

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

extern long long strtoclen(const char* s);
extern int strCmp(const void* s1, const void* s2);
extern char* currentdir(void);
extern char* cleanupName(char* name);
extern const char* expandPath(const char* name);

#ifndef HAVE_STRCHR
extern char* strchr(const char* s, int c);
#endif /* not HAVE_STRCHR */
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char* s1, const char* s2);
extern int strncasecmp(const char* s1, const char* s2, size_t n);
#endif /* not HAVE_STRCASECMP */
#ifndef HAVE_STRCASESTR
extern char* strcasestr(const char* s1, const char* s2);
#endif
extern int strcasemstr(char* str, char* srch[], char** ret_ptr);
int strmatchlen(const char* s1, const char* s2, int maxlen);
