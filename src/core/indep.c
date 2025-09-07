#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include "url.h"
#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include "indep.h"
#include "Str.h"
#include <gc.h>
#include "myctype.h"
#include "entity.h"

unsigned char QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24,
    72,
    76,
    40,
    8,
    40,
    41,
    77,
    72,
    72,
    72,
    40,
    72,
    8,
    0,
    64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    72,
    74,
    72,
    75,
    40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    72,
    72,
    72,
    72,
    24,

    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
};

char* HTML_QUOTE_MAP[] = {
    NULL,
    "&amp;",
    "&lt;",
    "&gt;",
    "&quot;",
    "&apos;",
    NULL,
    NULL,
};

long long
strtoclen(const char* s)
{
    return atoll(s);
}

int strCmp(const void* s1, const void* s2)
{
    return strcmp(*(const char**)s1, *(const char**)s2);
}

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

const char* expandPath(const char* name)
{
    struct passwd *passent, *getpwnam(const char*);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    const char* p = name;
    if (*p == '~') {
        p++;
        if (IS_ALPHA(*p)) {
            char* q = strchr(p, '/');
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
    return name;
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

const char* remove_space(const char* str)
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
    return p;
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

const char* html_quote(const char* str)
{
    Str tmp = NULL;

    const char* p;
    for (p = str; *p; p++) {
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
    return str;
}

char* html_unquote(char* str)
{
    Str tmp = NULL;
    char *p, *q;

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

const char* url_quote(const char* str)
{
    Str tmp = NULL;
    const char* p;
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

char* file_quote(char* str)
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
    char* p;

    for (p = str; *p; p++) {
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
    return str;
}


/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
