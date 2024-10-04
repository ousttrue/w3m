#pragma once

#define HAVE_STRCASECMP 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHR 1
#define HAVE_STRERROR 1

#ifdef _WIN32
extern char *strcasestr(const char *s1, const char *s2);
#else
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif
extern int strcasemstr(char *str, char *srch[], char **ret_ptr);
