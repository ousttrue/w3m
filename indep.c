#include "indep.h"
#include "alloc.h"
#include "quote.h"
#include "myctype.h"
#include "entity.h"
#include "Str.h"

#include <stdio.h>
#include <pwd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

char* currentdir()
{
    char* path;
#ifdef MAXPATHLEN
    path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
#else
    path = getcwd(NULL, 0);
#endif
    return path;
}

char* cleanupName(const char* name)
{
    char* buf = allocStr(name, -1);
    char* p = buf;
    const char* q = name;
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

char* expandPath(const char* name)
{
    struct passwd *passent, *getpwnam(const char*);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    const char* p = name;
    if (*p == '~') {
        p++;
        if (IS_ALPHA(*p)) {
            const char* q = strchr(p, '/');
            if (q) { /* ~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* ~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_charp(passent->pw_dir);
        } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
            extpath = Strnew_charp(getenv("HOME"));
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath->ptr;
    }
rest:
    return allocStr(name, -1);
}

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

char* remove_space(const char* str)
{
    const char *p, *q;
    for (p = str; *p && IS_SPACE(*p); p++)
        ;
    for (q = p; *q; q++)
        ;
    for (; q > p && IS_SPACE(*(q - 1)); q--)
        ;
    if (*q != '\0')
        return Strnew_charp_n(p, q - p)->ptr;
    return allocStr(p, -1);
}

int getescapechar(char** str)
{
    int dummy = -1;
    char *p = *str, *q;
    int strict_entity = true;

    if (*p == '&')
        p++;
    if (*p == '#') {
        p++;
        if (*p == 'x' || *p == 'X') {
            p++;
            if (!IS_XDIGIT(*p)) {
                *str = p;
                return -1;
            }
            for (dummy = GET_MYCDIGIT(*p), p++; IS_XDIGIT(*p); p++)
                dummy = dummy * 0x10 + GET_MYCDIGIT(*p);
            if (*p == ';')
                p++;
            *str = p;
            return dummy;
        } else {
            if (!IS_DIGIT(*p)) {
                *str = p;
                return -1;
            }
            for (dummy = GET_MYCDIGIT(*p), p++; IS_DIGIT(*p); p++)
                dummy = dummy * 10 + GET_MYCDIGIT(*p);
            if (*p == ';')
                p++;
            *str = p;
            return dummy;
        }
    }
    if (!IS_ALPHA(*p)) {
        *str = p;
        return -1;
    }
    q = p;
    for (p++; IS_ALNUM(*p); p++)
        ;
    q = allocStr(q, p - q);
    if (strcasestr("lt gt amp quot apos nbsp", q) && *p != '=') {
        /* a character entity MUST be terminated with ";". However,
         * there's MANY web pages which uses &lt , &gt or something
         * like them as &lt;, &gt;, etc. Therefore, we treat the most
         * popular character entities (including &#xxxx;) without
         * the last ";" as character entities. If the trailing character
         * is "=", it must be a part of query in an URL. So &lt=, &gt=, etc.
         * are not regarded as character entities.
         */
        strict_entity = false;
    }
    if (*p == ';')
        p++;
    else if (strict_entity) {
        *str = p;
        return -1;
    }
    *str = p;
    return getHash_si(&entity, q, -1);
}

char* getescapecmd(char** s)
{
    char* save = *s;
    Str tmp;
    int ch = getescapechar(s);

    if (ch >= 0)
        return conv_entity(ch);

    if (*save != '&')
        tmp = Strnew_charp("&");
    else
        tmp = Strnew();
    Strcat_charp_n(tmp, save, *s - save);
    return tmp->ptr;
}

char* html_quote(const char* str)
{
    Str tmp = NULL;
    for (const char* p = str; *p; p++) {
        char* q = html_quote_char(*p);
        if (q) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return allocStr(str, -1);
}

char* html_unquote(const char* str)
{
    Str tmp = NULL;
    const char *p, *q;

    for (p = str; *p;) {
        if (*p == '&') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            q = getescapecmd(&p);
            Strcat_charp(tmp, q);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
            p++;
        }
    }

    if (tmp)
        return tmp->ptr;
    return str;
}

static char xdigit[0x10] = "0123456789ABCDEF";

#define url_unquote_char(pstr) \
    ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : -1)

char* url_quote(const char* str)
{
    Str tmp = NULL;
    char* p;

    for (p = str; *p; p++) {
        if (is_url_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '%');
            Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
            Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

char* file_quote(const char* str)
{
    Str tmp = NULL;
    char* p;
    char buf[4];

    for (p = str; *p; p++) {
        if (is_file_quote(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            Strcat_charp(tmp, buf);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return allocStr(str, -1);
}

char* file_unquote(const char* str)
{
    Str tmp = NULL;
    char *p, *q;
    int c;

    for (p = str; *p;) {
        if (*p == '%') {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0) {
                if (tmp == NULL)
                    tmp = Strnew_charp_n(str, (int)(p - str));
                if (c != '\0' && c != '\n' && c != '\r')
                    Strcat_char(tmp, (char)c);
                p = q;
                continue;
            }
        }
        if (tmp)
            Strcat_char(tmp, *p);
        p++;
    }
    if (tmp)
        return tmp->ptr;
    return str;
}

Str Str_form_quote(Str x)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length;
    char buf[4];

    for (; p < ep; p++) {
        if (*p == ' ') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            Strcat_char(tmp, '+');
        } else if (is_url_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            sprintf(buf, "%%%02X", (unsigned char)*p);
            Strcat_charp(tmp, buf);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return x;
}

Str Str_url_unquote(Str x, int is_form, int safe)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length, *q;
    int c;

    for (; p < ep;) {
        if (is_form && *p == '+') {
            if (tmp == NULL)
                tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
            Strcat_char(tmp, ' ');
            p++;
            continue;
        } else if (*p == '%') {
            q = p;
            c = url_unquote_char(&q);
            if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
                if (tmp == NULL)
                    tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
                Strcat_char(tmp, (char)c);
                p = q;
                continue;
            }
        }
        if (tmp)
            Strcat_char(tmp, *p);
        p++;
    }
    if (tmp)
        return tmp;
    return x;
}

char* shell_quote(const char* str)
{
    Str tmp = NULL;
    for (const char* p = str; *p; p++) {
        if (is_shell_unsafe(*p)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(str, (int)(p - str));
            Strcat_char(tmp, '\\');
            Strcat_char(tmp, *p);
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp->ptr;
    return allocStr(str, -1);
}
