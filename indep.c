#include "indep.h"
#include "entity.h"
#include "fm.h"
#include <stdio.h>
#include <pwd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include <gcstr.h>
#include <unistd.h>

long long
strtoclen(const char* s)
{
#ifdef HAVE_STRTOLL
    return strtoll(s, NULL, 10);
#elif defined(HAVE_STRTOQ)
    return strtoq(s, NULL, 10);
#elif defined(HAVE_ATOLL)
    return atoll(s);
#elif defined(HAVE_ATOQ)
    return atoq(s);
#else
    return atoi(s);
#endif
}

int strCmp(const void* s1, const void* s2)
{
    return strcmp(*(const char**)s1, *(const char**)s2);
}

char* currentdir()
{
    char* path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
    path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
#else
    path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
    path = NewAtom_N(char, 1024);
    getwd(path);
#else /* not HAVE_GETWD */
    FILE* f;
    char* p;
    path = NewAtom_N(char, 1024);
    f = popen("pwd", "r");
    fgets(path, 1024, f);
    pclose(f);
    for (p = path; *p; p++)
        if (*p == '\n') {
            *p = '\0';
            break;
        }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
    return path;
}

char* cleanupName(const char* name)
{
    char *buf, *p, *q;

    buf = allocStr(name, -1);
    p = buf;
    q = name;
    while (*q != '\0') {
        if (strncmp(p, "/../", 4) == 0) { /* foo/bar/../FOO */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
                /* ../../       */
                p += 3;
                q += 3;
            } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../../    */
                p += 3;
                q += 3;
            } else {
                while (p != buf && *--p != '/')
                    ; /* ->foo/FOO */
                *p = '\0';
                q += 3;
                strcat(buf, q);
            }
        } else if (strcmp(p, "/..") == 0) { /* foo/bar/..   */
            if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
                /* ../..        */
            } else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
                /* ../../..     */
            } else {
                while (p != buf && *--p != '/')
                    ; /* ->foo/ */
                *++p = '\0';
            }
            break;
        } else if (strncmp(p, "/./", 3) == 0) { /* foo/./bar */
            *p = '\0'; /* -> foo/bar           */
            q += 2;
            strcat(buf, q);
        } else if (strcmp(p, "/.") == 0) { /* foo/. */
            *++p = '\0'; /* -> foo/              */
            break;
        } else if (strncmp(p, "//", 2) == 0) { /* foo//bar */
            /* -> foo/bar           */
            *p = '\0';
            q++;
            strcat(buf, q);
        } else {
            p++;
            q++;
        }
    }
    return buf;
}

#ifndef HAVE_STRCHR
char* strchr(const char* s, int c)
{
    while (*s) {
        if ((unsigned char)*s == c)
            return (char*)s;
        s++;
    }
    return NULL;
}
#endif /* not HAVE_STRCHR */

#ifndef HAVE_STRCASECMP
int strcasecmp(const char* s1, const char* s2)
{
    int x;
    while (*s1) {
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            return x;
        s1++;
        s2++;
    }
    return -TOLOWER(*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n)
{
    int x;
    while (*s1 && n) {
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            return x;
        s1++;
        s2++;
        n--;
    }
    return n ? -TOLOWER(*s2) : 0;
}
#endif /* not HAVE_STRCASECMP */

#ifndef HAVE_STRCASESTR
/* string search using the simplest algorithm */
char* strcasestr(const char* s1, const char* s2)
{
    int len1, len2;
    if (s2 == NULL)
        return (char*)s1;
    if (*s2 == '\0')
        return (char*)s1;
    len1 = strlen(s1);
    len2 = strlen(s2);
    while (*s1 && len1 >= len2) {
        if (strncasecmp(s1, s2, len2) == 0)
            return (char*)s1;
        s1++;
        len1--;
    }
    return 0;
}
#endif

static int
strcasematch(char* s1, char* s2)
{
    int x;
    while (*s1) {
        if (*s2 == '\0')
            return 1;
        x = TOLOWER(*s1) - TOLOWER(*s2);
        if (x != 0)
            break;
        s1++;
        s2++;
    }
    return (*s2 == '\0');
}

/* search multiple strings */
int strcasemstr(char* str, char* srch[], char** ret_ptr)
{
    int i;
    while (*str) {
        for (i = 0; srch[i]; i++) {
            if (strcasematch(str, srch[i])) {
                if (ret_ptr)
                    *ret_ptr = str;
                return i;
            }
        }
        str++;
    }
    return -1;
}

int strmatchlen(const char* s1, const char* s2, int maxlen)
{
    int i;

    /* To allow the maxlen to be negatie (infinity),
     * compare by "!=" instead of "<=". */
    for (i = 0; i != maxlen; ++i) {
        if (!s1[i] || !s2[i] || s1[i] != s2[i])
            break;
    }
    return i;
}
